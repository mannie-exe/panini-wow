#include <windows.h>
#include <d3d9.h>
#include <psapi.h>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cstdlib>

#define PROBE_LOG(fmt, ...) \
    do { if (g_logFile) { fprintf(g_logFile, "[INFO] [probe] " fmt "\n", ##__VA_ARGS__); fflush(g_logFile); } } while(0)

static FILE* g_logFile = nullptr;

static inline uint32_t FloatBits(float f) {
    uint32_t u;
    memcpy(&u, &f, sizeof(u));
    return u;
}

static bool IsReadable(const void* addr, size_t len) {
    MEMORY_BASIC_INFORMATION mbi;
    if (VirtualQuery(addr, &mbi, sizeof(mbi)) == 0) return false;
    if (mbi.State != MEM_COMMIT) return false;
    auto start = (uintptr_t)addr;
    auto end = start + len;
    auto regionEnd = (uintptr_t)mbi.BaseAddress + mbi.RegionSize;
    return end <= regionEnd;
}

// ---------------------------------------------------------------------------
// Offset tables (must match include/wow_offsets.h in the main project)
// ---------------------------------------------------------------------------

enum class WowVersion { Classic112, WotLK335 };

struct WowOffsets {
    WowVersion version;
    uintptr_t CVarLookup;
    uintptr_t CVarRegister;
    uintptr_t CVar_FloatValue;
    uintptr_t CVar_IntValue;
    uintptr_t WorldFrame_Ptr;
    uintptr_t ActiveCamera_Off;
    uintptr_t Camera_FOV_Off;
    uintptr_t CGxDeviceD3d_Ptr;
    uintptr_t D3DDevice_Off;
    uintptr_t RenderWorld_Addr;
    uintptr_t GetActiveCamera_Addr;
    uintptr_t CVarGetString_Addr;
    uintptr_t CVarSet_Addr;
};

static constexpr WowOffsets kClassic = {
    .version              = WowVersion::Classic112,
    .CVarLookup           = 0x0063DEC0,
    .CVarRegister         = 0x0063DB90,
    .CVar_FloatValue      = 0x24,
    .CVar_IntValue        = 0x28,
    .WorldFrame_Ptr       = 0x00B4B2BC,
    .ActiveCamera_Off     = 0x65B8,
    .Camera_FOV_Off       = 0x40,
    .CGxDeviceD3d_Ptr     = 0x00C0ED38,
    .D3DDevice_Off        = 0x38A8,
    .RenderWorld_Addr     = 0x00482D70,
    .GetActiveCamera_Addr = 0,
    .CVarGetString_Addr   = 0,
    .CVarSet_Addr         = 0,
};

// WotLK CVar functions are __cdecl wrappers that load the CVarSystem singleton
// (0x00CA19FC) internally. RenderWorld at 0x4FAF90 is a __cdecl per-frame
// callback scheduled by OnFrameRender (vtable[32]).
static constexpr WowOffsets kWotLK = {
    .version              = WowVersion::WotLK335,
    .CVarLookup           = 0x00767440,
    .CVarRegister         = 0x00767FC0,
    .CVar_FloatValue      = 0,
    .CVar_IntValue        = 0,
    .WorldFrame_Ptr       = 0x00B7436C,
    .ActiveCamera_Off     = 0,
    .Camera_FOV_Off       = 0x40,
    .CGxDeviceD3d_Ptr     = 0x00C5DF88,
    .D3DDevice_Off        = 0x397C,
    .RenderWorld_Addr     = 0x004FAF90,
    .GetActiveCamera_Addr = 0x004F5960,
    .CVarGetString_Addr   = 0x00767460,
    .CVarSet_Addr         = 0x007668C0,
};

typedef void* (__cdecl* GetActiveCameraFn)();
typedef HRESULT (__stdcall* EndScene_t)(IDirect3DDevice9*);
typedef HRESULT (__stdcall* Reset_t)(IDirect3DDevice9*, D3DPRESENT_PARAMETERS*);

static bool g_probed = false;
static EndScene_t g_originalEndScene = nullptr;
static Reset_t g_originalReset = nullptr;
static IDirect3DDevice9* g_device = nullptr;
static const WowOffsets* g_offsets = nullptr;

// ---------------------------------------------------------------------------
// Version detection
// ---------------------------------------------------------------------------

static WowVersion DetectVersion() {
    HMODULE hExe = GetModuleHandleA(NULL);
    if (!hExe) {
        g_offsets = &kClassic;
        return WowVersion::Classic112;
    }

    auto dos = reinterpret_cast<IMAGE_DOS_HEADER*>(hExe);
    auto nt = reinterpret_cast<IMAGE_NT_HEADERS*>(
        reinterpret_cast<uint8_t*>(hExe) + dos->e_lfanew);

    DWORD timestamp = nt->FileHeader.TimeDateStamp;
    DWORD exeSize = nt->OptionalHeader.SizeOfImage;

    PROBE_LOG("Exe base: 0x%08X", (uintptr_t)hExe);
    PROBE_LOG("Exe size: 0x%08X", exeSize);
    PROBE_LOG("PE timestamp: 0x%08X", timestamp);

    if (timestamp >= 0x48000000) {
        g_offsets = &kWotLK;
        PROBE_LOG("Version: WotLK 3.3.5a (build 12340)");
        return WowVersion::WotLK335;
    } else {
        g_offsets = &kClassic;
        PROBE_LOG("Version: Classic 1.12.1 (build 5875)");
        return WowVersion::Classic112;
    }
}

// ---------------------------------------------------------------------------
// Offset validation
// ---------------------------------------------------------------------------

static void CheckFuncAddr(const char* name, uintptr_t addr, int& checked, int& valid) {
    checked++;
    if (!addr) {
        PROBE_LOG("%-18s: (not used on this version)", name);
        return;
    }
    if (!IsReadable((void*)addr, 8)) {
        PROBE_LOG("%-18s @ 0x%08X: NOT READABLE", name, addr);
        return;
    }
    auto* p = reinterpret_cast<uint8_t*>(addr);
    uint8_t bytes[8];
    memcpy(bytes, p, 8);

    bool allZero = true, allInt3 = true;
    for (int i = 0; i < 8; i++) {
        if (bytes[i] != 0x00) allZero = false;
        if (bytes[i] != 0xCC) allInt3 = false;
    }

    if (allZero)     PROBE_LOG("%-18s @ 0x%08X: ZERO (bad)", name, addr);
    else if (allInt3) PROBE_LOG("%-18s @ 0x%08X: INT3 (bad)", name, addr);
    else {
        PROBE_LOG("%-18s @ 0x%08X: [%02X %02X %02X %02X %02X %02X %02X %02X] OK",
            name, addr, bytes[0], bytes[1], bytes[2], bytes[3],
            bytes[4], bytes[5], bytes[6], bytes[7]);
        valid++;
    }
}

static void CheckPtrAddr(const char* name, uintptr_t addr, int& checked, int& valid) {
    checked++;
    if (!addr) {
        PROBE_LOG("%-18s: (not used on this version)", name);
        return;
    }
    if (!IsReadable((void*)addr, sizeof(uintptr_t))) {
        PROBE_LOG("%-18s @ 0x%08X: NOT READABLE", name, addr);
        return;
    }
    auto val = *reinterpret_cast<uintptr_t*>(addr);
    if (val == 0)
        PROBE_LOG("%-18s @ 0x%08X: value 0x00000000 (null, may be pre-init)", name, addr);
    else {
        PROBE_LOG("%-18s @ 0x%08X: value 0x%08X OK", name, addr, val);
        valid++;
    }
}

static void ValidateOffsets() {
    PROBE_LOG("=== Offset Validation ===");
    int checked = 0, valid = 0;

    CheckFuncAddr("CVarLookup",    g_offsets->CVarLookup, checked, valid);
    CheckFuncAddr("CVarRegister",  g_offsets->CVarRegister, checked, valid);
    CheckFuncAddr("CVarGetString", g_offsets->CVarGetString_Addr, checked, valid);
    CheckFuncAddr("CVarSet",       g_offsets->CVarSet_Addr, checked, valid);
    CheckFuncAddr("GetActiveCam",  g_offsets->GetActiveCamera_Addr, checked, valid);
    CheckFuncAddr("RenderWorld",   g_offsets->RenderWorld_Addr, checked, valid);
    CheckPtrAddr ("WorldFrame_Ptr", g_offsets->WorldFrame_Ptr, checked, valid);
    CheckPtrAddr ("CGxDeviceD3d",  g_offsets->CGxDeviceD3d_Ptr, checked, valid);
    PROBE_LOG("%-18s: 0x%04X (offset, not probed)", "D3DDevice_Off",
              g_offsets->D3DDevice_Off);

    PROBE_LOG("Result: %d/%d valid", valid, checked);
}

// ---------------------------------------------------------------------------
// D3D9 device probe
// ---------------------------------------------------------------------------

static IDirect3DDevice9* GetWoWDevice() {
    if (!IsReadable((void*)g_offsets->CGxDeviceD3d_Ptr, sizeof(uintptr_t)))
        return nullptr;
    auto pGx = *reinterpret_cast<uint8_t**>(g_offsets->CGxDeviceD3d_Ptr);
    if (!pGx) return nullptr;
    if (!IsReadable(pGx + g_offsets->D3DDevice_Off, sizeof(uintptr_t)))
        return nullptr;
    return *reinterpret_cast<IDirect3DDevice9**>(pGx + g_offsets->D3DDevice_Off);
}

static constexpr int VTABLE_ENDSCENE = 42;
static constexpr int VTABLE_RESET    = 16;

static HRESULT __stdcall ProbeEndScene(IDirect3DDevice9* dev);
static HRESULT __stdcall ProbeReset(IDirect3DDevice9* dev, D3DPRESENT_PARAMETERS* params);

static void ProbeD3DDevice() {
    PROBE_LOG("=== D3D9 Device ===");

    IDirect3DDevice9* dev = nullptr;
    for (int i = 0; i < 600 && !dev; i++) {
        dev = GetWoWDevice();
        if (!dev) Sleep(100);
    }

    if (!dev) {
        PROBE_LOG("WoW D3D9 device not found after 60s");
        return;
    }
    PROBE_LOG("Device: 0x%08X", (uintptr_t)dev);

    if (!IsReadable(dev, sizeof(void*))) {
        PROBE_LOG("vtable not readable");
        return;
    }

    void** vtable = *reinterpret_cast<void***>(dev);
    PROBE_LOG("vtable: 0x%08X", (uintptr_t)vtable);

    if (IsReadable(vtable + VTABLE_ENDSCENE, sizeof(void*)))
        PROBE_LOG("EndScene (vtable[42]): 0x%08X", (uintptr_t)vtable[VTABLE_ENDSCENE]);
    if (IsReadable(vtable + VTABLE_RESET, sizeof(void*)))
        PROBE_LOG("Reset    (vtable[16]): 0x%08X", (uintptr_t)vtable[VTABLE_RESET]);

    g_originalEndScene = reinterpret_cast<EndScene_t>(vtable[VTABLE_ENDSCENE]);
    g_originalReset = reinterpret_cast<Reset_t>(vtable[VTABLE_RESET]);
    g_device = dev;

    DWORD oldProt;
    VirtualProtect(&vtable[VTABLE_ENDSCENE], sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProt);
    vtable[VTABLE_ENDSCENE] = reinterpret_cast<void*>(&ProbeEndScene);
    VirtualProtect(&vtable[VTABLE_ENDSCENE], sizeof(void*), oldProt, &oldProt);

    VirtualProtect(&vtable[VTABLE_RESET], sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProt);
    vtable[VTABLE_RESET] = reinterpret_cast<void*>(&ProbeReset);
    VirtualProtect(&vtable[VTABLE_RESET], sizeof(void*), oldProt, &oldProt);

    PROBE_LOG("EndScene hook installed (temporary, restores after world load)");

    IDirect3DSurface9* pRT = nullptr;
    if (SUCCEEDED(dev->GetRenderTarget(0, &pRT)) && pRT) {
        D3DSURFACE_DESC desc;
        pRT->GetDesc(&desc);
        PROBE_LOG("Backbuffer: %ux%u fmt=%u", desc.Width, desc.Height, desc.Format);
        pRT->Release();
    }
}

// ---------------------------------------------------------------------------
// Camera probe
// ---------------------------------------------------------------------------

static void ProbeCamera() {
    PROBE_LOG("=== Camera ===");

    if (g_offsets->version == WowVersion::Classic112) {
        if (!IsReadable((void*)kClassic.WorldFrame_Ptr, sizeof(uintptr_t))) {
            PROBE_LOG("WorldFrame_Ptr not readable");
            return;
        }
        auto pWF = *reinterpret_cast<uintptr_t*>(kClassic.WorldFrame_Ptr);
        PROBE_LOG("WorldFrame: 0x%08X", pWF);
        if (pWF && IsReadable((void*)(pWF + kClassic.ActiveCamera_Off), sizeof(uintptr_t))) {
            auto pCam = *reinterpret_cast<uintptr_t*>(pWF + kClassic.ActiveCamera_Off);
            PROBE_LOG("Camera: 0x%08X (WorldFrame+0x%04X)", pCam, kClassic.ActiveCamera_Off);
            if (pCam && IsReadable((void*)(pCam + kClassic.Camera_FOV_Off), sizeof(float))) {
                float fov = *reinterpret_cast<float*>(pCam + kClassic.Camera_FOV_Off);
                PROBE_LOG("FoV: 0x%08X (camera+0x40)", FloatBits(fov));
            }
        }
    } else {
        if (!IsReadable((void*)kWotLK.GetActiveCamera_Addr, 8)) {
            PROBE_LOG("GetActiveCamera addr not readable");
            return;
        }
        auto getCam = reinterpret_cast<GetActiveCameraFn>(kWotLK.GetActiveCamera_Addr);
        auto pCam = reinterpret_cast<uintptr_t>(getCam());
        PROBE_LOG("Camera: 0x%08X (via GetActiveCamera at 0x%08X)",
                  pCam, kWotLK.GetActiveCamera_Addr);
        if (pCam && IsReadable((void*)(pCam + kWotLK.Camera_FOV_Off), sizeof(float))) {
            float fov = *reinterpret_cast<float*>(pCam + kWotLK.Camera_FOV_Off);
            PROBE_LOG("FoV: 0x%08X (camera+0x40)", FloatBits(fov));
        }
    }
}

// ---------------------------------------------------------------------------
// CVar probe
// ---------------------------------------------------------------------------

static void ProbeCVars() {
    PROBE_LOG("=== CVar System ===");

    // Test with a known WoW CVar that exists on both versions.
    const char* testName = "realmName";

    if (g_offsets->version == WowVersion::Classic112) {
        auto lookupFn = reinterpret_cast<uintptr_t* (__fastcall*)(const char*)>(
            g_offsets->CVarLookup);
        auto* cv = lookupFn(testName);
        if (cv && IsReadable(cv, 0x30)) {
            float fval = *reinterpret_cast<float*>(
                reinterpret_cast<uint8_t*>(cv) + kClassic.CVar_FloatValue);
            PROBE_LOG("CVarLookup(\"%s\"): struct at 0x%08X, float+0x24=0x%08X",
                      testName, (uintptr_t)cv, FloatBits(fval));
        } else {
            PROBE_LOG("CVarLookup(\"%s\"): null (CVar system may not be ready)", testName);
        }
    } else {
        // WotLK: __cdecl wrapper, loads singleton internally
        auto lookupFn = reinterpret_cast<void* (__cdecl*)(const char*)>(
            g_offsets->CVarLookup);
        auto* cv = lookupFn(testName);
        if (cv && IsReadable(cv, 0x30)) {
            auto str = *reinterpret_cast<const char**>(
                reinterpret_cast<uint8_t*>(cv) + 0x28);
            PROBE_LOG("CVarLookup(\"%s\"): struct at 0x%08X, string+0x28=\"%s\"",
                      testName, (uintptr_t)cv, str ? str : "(null)");
        } else {
            PROBE_LOG("CVarLookup(\"%s\"): null (CVar system may not be ready)", testName);
        }
    }
}

// ---------------------------------------------------------------------------
// Module listing
// ---------------------------------------------------------------------------

static void ProbeModules() {
    PROBE_LOG("=== Loaded Modules ===");

    HMODULE modules[256];
    DWORD needed = 0;
    if (!EnumProcessModules(GetCurrentProcess(), modules, sizeof(modules), &needed)) {
        PROBE_LOG("EnumProcessModules failed");
        return;
    }

    int count = needed / sizeof(HMODULE);
    for (int i = 0; i < count; i++) {
        char name[MAX_PATH] = {};
        if (GetModuleBaseNameA(GetCurrentProcess(), modules[i], name, MAX_PATH)) {
            if (_stricmp(name, "d3d9.dll") == 0) {
                char fullPath[MAX_PATH] = {};
                GetModuleFileNameA(modules[i], fullPath, MAX_PATH);
                PROBE_LOG("%-30s @ 0x%08X  %s", name, (uintptr_t)modules[i], fullPath);
            } else {
                PROBE_LOG("%-30s @ 0x%08X", name, (uintptr_t)modules[i]);
            }
        }
    }
    PROBE_LOG("Total: %d modules", count);
}

// ---------------------------------------------------------------------------
// Deferred probes (run once after world loads via EndScene hook)
// ---------------------------------------------------------------------------

static HRESULT __stdcall ProbeEndScene(IDirect3DDevice9* dev) {
    if (g_probed)
        return g_originalEndScene(dev);

    // Match the main DLL's world-ready detection per version:
    // Classic checks WorldFrame_Ptr != null; WotLK checks GetActiveCamera() != null.
    bool worldReady = false;
    if (g_offsets->version == WowVersion::WotLK335) {
        if (IsReadable((void*)g_offsets->GetActiveCamera_Addr, 8)) {
            auto getCam = reinterpret_cast<GetActiveCameraFn>(g_offsets->GetActiveCamera_Addr);
            worldReady = getCam() != nullptr;
        }
    } else {
        if (IsReadable((void*)g_offsets->WorldFrame_Ptr, sizeof(uintptr_t)))
            worldReady = *reinterpret_cast<uintptr_t*>(g_offsets->WorldFrame_Ptr) != 0;
    }
    if (!worldReady)
        return g_originalEndScene(dev);

    g_probed = true;

    g_logFile = fopen("mods\\Probe.log", "a");
    if (!g_logFile) g_logFile = fopen("Probe.log", "a");

    PROBE_LOG("=== Deferred Probes (world loaded) ===");

    ProbeCVars();
    ProbeCamera();

    PROBE_LOG("=== Deferred Probes Complete ===");

    // Restore original vtable entries.
    if (g_device) {
        void** vtable = *reinterpret_cast<void***>(g_device);
        DWORD oldProt;
        VirtualProtect(&vtable[VTABLE_ENDSCENE], sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProt);
        vtable[VTABLE_ENDSCENE] = reinterpret_cast<void*>(g_originalEndScene);
        VirtualProtect(&vtable[VTABLE_ENDSCENE], sizeof(void*), oldProt, &oldProt);
        VirtualProtect(&vtable[VTABLE_RESET], sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProt);
        vtable[VTABLE_RESET] = reinterpret_cast<void*>(g_originalReset);
        VirtualProtect(&vtable[VTABLE_RESET], sizeof(void*), oldProt, &oldProt);
        PROBE_LOG("vtable restored");
    }

    fclose(g_logFile);
    g_logFile = nullptr;

    return g_originalEndScene(dev);
}

static HRESULT __stdcall ProbeReset(IDirect3DDevice9* dev, D3DPRESENT_PARAMETERS* params) {
    return g_originalReset(dev, params);
}

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------

static DWORD WINAPI InitThread(LPVOID) {
    g_logFile = fopen("mods\\Probe.log", "w");
    if (!g_logFile) g_logFile = fopen("Probe.log", "w");
    if (!g_logFile) return 1;

    PROBE_LOG("Probe.dll v0.2.0 starting");

    PROBE_LOG("=== Version Detection ===");
    DetectVersion();

    ValidateOffsets();
    ProbeD3DDevice();
    ProbeCamera();
    ProbeModules();

    PROBE_LOG("=== Initial probes complete (deferred probes run when world loads) ===");

    // ProbeEndScene may have already closed g_logFile on the render thread
    // if the world was loaded when the probe was injected.
    if (g_logFile) {
        fclose(g_logFile);
        g_logFile = nullptr;
    }
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID) {
    switch (reason) {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hModule);
        CreateThread(NULL, 0, InitThread, NULL, 0, NULL);
        return TRUE;
    }
    return TRUE;
}
