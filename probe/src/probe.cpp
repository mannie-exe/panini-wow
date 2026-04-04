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

// Safe memory read: returns true if addr points to readable memory.
static bool IsReadable(const void* addr, size_t len) {
    MEMORY_BASIC_INFORMATION mbi;
    if (VirtualQuery(addr, &mbi, sizeof(mbi)) == 0) return false;
    if (mbi.State != MEM_COMMIT) return false;
    auto start = (uintptr_t)addr;
    auto end = start + len;
    auto regionEnd = (uintptr_t)mbi.BaseAddress + mbi.RegionSize;
    return end <= regionEnd;
}

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
    .RenderWorld_Addr     = 0x780F50,
    .GetActiveCamera_Addr = 0x004F5960,
    .CVarGetString_Addr   = 0x00767460,
    .CVarSet_Addr         = 0x007668C0,
};

struct WotLKCvar { char _pad[0x28]; const char* stringValue; };
typedef void* (__cdecl* GetActiveCameraFn)();

typedef HRESULT (__stdcall* EndScene_t)(IDirect3DDevice9*);
typedef HRESULT (__stdcall* Reset_t)(IDirect3DDevice9*, D3DPRESENT_PARAMETERS*);

static bool g_probed = false;
static EndScene_t g_originalEndScene = nullptr;
static Reset_t g_originalReset = nullptr;
static IDirect3DDevice9* g_device = nullptr;

static const WowOffsets* g_offsets = nullptr;

static uintptr_t GetWorldFramePtr() {
    if (!IsReadable((void*)g_offsets->WorldFrame_Ptr, sizeof(uintptr_t)))
        return 0;
    return *reinterpret_cast<uintptr_t*>(g_offsets->WorldFrame_Ptr);
}

static HRESULT __stdcall ProbeEndScene(IDirect3DDevice9* dev);
static HRESULT __stdcall ProbeReset(IDirect3DDevice9* dev, D3DPRESENT_PARAMETERS* params);

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

    // PE timestamp threshold 0x48000000 (~March 2008) separates Classic from WotLK
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

static void CheckFuncAddr(const char* name, uintptr_t addr, int& checked, int& valid) {
    checked++;
    if (!IsReadable((void*)addr, 8)) {
        PROBE_LOG("%-18s @ 0x%08X: not readable", name, addr);
        return;
    }
    auto* p = reinterpret_cast<uint8_t*>(addr);
    uint8_t bytes[8] = {};
    memcpy(bytes, p, 8);

    bool allZero = true;
    bool allInt3 = true;
    for (int i = 0; i < 8; i++) {
        if (bytes[i] != 0x00) allZero = false;
        if (bytes[i] != 0xCC) allInt3 = false;
    }

    if (allZero) {
        PROBE_LOG("%-18s @ 0x%08X: ZERO", name, addr);
    } else if (allInt3) {
        PROBE_LOG("%-18s @ 0x%08X: INT3", name, addr);
    } else {
        PROBE_LOG("%-18s @ 0x%08X: first bytes [%02X %02X %02X %02X %02X %02X %02X %02X] VALID",
            name, addr,
            bytes[0], bytes[1], bytes[2], bytes[3],
            bytes[4], bytes[5], bytes[6], bytes[7]);
        valid++;
    }
}

static void CheckPtrAddr(const char* name, uintptr_t addr, int& checked, int& valid) {
    checked++;
    if (!IsReadable((void*)addr, sizeof(uintptr_t))) {
        PROBE_LOG("%-18s @ 0x%08X: not readable", name, addr);
        return;
    }
    auto val = *reinterpret_cast<uintptr_t*>(addr);
    if (val == 0) {
        PROBE_LOG("%-18s @ 0x%08X: value 0x00000000 ZERO", name, addr);
    } else {
        PROBE_LOG("%-18s @ 0x%08X: value 0x%08X", name, addr, val);
        valid++;
    }
}

static void ValidateOffsets() {
    PROBE_LOG("=== Offset Validation ===");
    int checked = 0;
    int valid = 0;

    if (g_offsets->version == WowVersion::Classic112) {
        CheckFuncAddr("CVarLookup",   kClassic.CVarLookup, checked, valid);
        CheckFuncAddr("CVarRegister", kClassic.CVarRegister, checked, valid);
        CheckPtrAddr ("WorldFrame_Ptr", kClassic.WorldFrame_Ptr, checked, valid);
        CheckPtrAddr ("CGxDeviceD3d",  kClassic.CGxDeviceD3d_Ptr, checked, valid);
        checked++;
        PROBE_LOG("%-18s: 0x%04X (offset, not probed)", "D3DDevice_Off", kClassic.D3DDevice_Off);
        CheckFuncAddr("RenderWorld",  kClassic.RenderWorld_Addr, checked, valid);
    } else {
        CheckFuncAddr("CVarLookup",   kWotLK.CVarLookup, checked, valid);
        CheckFuncAddr("CVarRegister", kWotLK.CVarRegister, checked, valid);
        CheckFuncAddr("CVarGetString", kWotLK.CVarGetString_Addr, checked, valid);
        CheckFuncAddr("CVar__Set",     kWotLK.CVarSet_Addr, checked, valid);
        CheckFuncAddr("GetActiveCam",  kWotLK.GetActiveCamera_Addr, checked, valid);
        CheckPtrAddr ("WorldFrame_Ptr", kWotLK.WorldFrame_Ptr, checked, valid);
        CheckPtrAddr ("CGxDeviceD3d",  kWotLK.CGxDeviceD3d_Ptr, checked, valid);
        checked++;
        PROBE_LOG("%-18s: 0x%04X (offset, not probed)", "D3DDevice_Off", kWotLK.D3DDevice_Off);
        CheckFuncAddr("RenderWorld",   kWotLK.RenderWorld_Addr, checked, valid);
    }

    PROBE_LOG("%d/%d offsets checked", valid, checked);
}

static IDirect3DDevice9* GetWoWDevice() {
    if (!IsReadable((void*)g_offsets->CGxDeviceD3d_Ptr, sizeof(uintptr_t)))
        return nullptr;
    auto pGx = *reinterpret_cast<uint8_t**>(g_offsets->CGxDeviceD3d_Ptr);
    if (!pGx) return nullptr;
    if (!IsReadable(pGx + g_offsets->D3DDevice_Off, sizeof(uintptr_t)))
        return nullptr;
    return *reinterpret_cast<IDirect3DDevice9**>(pGx + g_offsets->D3DDevice_Off);
}

// EndScene is vtable index 42 in IDirect3DDevice9
static constexpr int VTABLE_ENDSCENE = 42;
// Reset is vtable index 16 in IDirect3DDevice9
static constexpr int VTABLE_RESET = 16;

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

    PROBE_LOG("WoW D3D9 device: 0x%08X", (uintptr_t)dev);

    if (!IsReadable(dev, sizeof(void*))) {
        PROBE_LOG("vtable not readable");
        return;
    }

    void** vtable = *reinterpret_cast<void***>(dev);
    PROBE_LOG("vtable base: 0x%08X", (uintptr_t)vtable);

    if (IsReadable(vtable + VTABLE_ENDSCENE, sizeof(void*)))
        PROBE_LOG("EndScene (vtable[42]): 0x%08X", (uintptr_t)vtable[VTABLE_ENDSCENE]);
    else
        PROBE_LOG("EndScene (vtable[42]): not readable");

    if (IsReadable(vtable + VTABLE_RESET, sizeof(void*)))
        PROBE_LOG("Reset    (vtable[16]): 0x%08X", (uintptr_t)vtable[VTABLE_RESET]);
    else
        PROBE_LOG("Reset    (vtable[16]): not readable");

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

    PROBE_LOG("EndScene + Reset hooks installed (temporary, will restore after world load)");

    IDirect3DSurface9* pRT = nullptr;
    HRESULT hr = dev->GetRenderTarget(0, &pRT);
    if (SUCCEEDED(hr) && pRT) {
        D3DSURFACE_DESC desc;
        pRT->GetDesc(&desc);
        PROBE_LOG("Backbuffer: %ux%u", desc.Width, desc.Height);
        pRT->Release();
    } else {
        PROBE_LOG("GetRenderTarget(0) failed hr=0x%08X", (unsigned)hr);
    }
}

static void ProbeCamera() {
    PROBE_LOG("=== Camera ===");

    if (g_offsets->version == WowVersion::Classic112) {
        if (!IsReadable((void*)kClassic.WorldFrame_Ptr, sizeof(uintptr_t))) {
            PROBE_LOG("WorldFrame_Ptr not readable");
            return;
        }
        auto pWF = *reinterpret_cast<uintptr_t*>(kClassic.WorldFrame_Ptr);
        PROBE_LOG("WorldFrame ptr: 0x%08X", pWF);
        if (pWF && IsReadable((void*)(pWF + kClassic.ActiveCamera_Off), sizeof(uintptr_t))) {
            auto pCam = *reinterpret_cast<uintptr_t*>(pWF + kClassic.ActiveCamera_Off);
            PROBE_LOG("Camera ptr: 0x%08X", pCam);
            if (pCam && IsReadable((void*)(pCam + kClassic.Camera_FOV_Off), sizeof(float))) {
                float fov = *reinterpret_cast<float*>(pCam + kClassic.Camera_FOV_Off);
                PROBE_LOG("FoV (camera+0x40): 0x%08X", FloatBits(fov));
            }
        }
    } else {
        auto getCam = reinterpret_cast<GetActiveCameraFn>(kWotLK.GetActiveCamera_Addr);
        if (!IsReadable((void*)kWotLK.GetActiveCamera_Addr, 8)) {
            PROBE_LOG("GetActiveCamera addr not readable");
            return;
        }
        auto pCam = reinterpret_cast<uintptr_t>(getCam());
        PROBE_LOG("Camera ptr (GetActiveCamera): 0x%08X", pCam);
        if (pCam && IsReadable((void*)(pCam + kWotLK.Camera_FOV_Off), sizeof(float))) {
            float fov = *reinterpret_cast<float*>(pCam + kWotLK.Camera_FOV_Off);
            PROBE_LOG("FoV (camera+0x40): 0x%08X", FloatBits(fov));
        }
    }
}

struct FakeCVar { uint8_t data[0x30]; };
static FakeCVar s_fakeCvar = {};

static void InitFakeCVar() {
    memset(&s_fakeCvar, 0, sizeof(s_fakeCvar));
    if (g_offsets->version == WowVersion::WotLK335) {
        static const char* fakeStr = "1.0";
        *reinterpret_cast<const char**>(s_fakeCvar.data + 0x28) = fakeStr;
    } else {
        float val = 1.0f;
        memcpy(s_fakeCvar.data + 0x24, &val, sizeof(val));
        int ival = 1;
        memcpy(s_fakeCvar.data + 0x28, &ival, sizeof(ival));
    }
}

static void TestCVarIntercept() {
    PROBE_LOG("=== CVar Intercept Test ===");

    bool isWotLK = g_offsets->version == WowVersion::WotLK335;

    if (isWotLK) {
        auto getFn = reinterpret_cast<WotLKCvar* (__cdecl*)(const char*)>(g_offsets->CVarGetString_Addr);

        auto* cv = getFn("probe_test");
        if (cv && IsReadable(cv, 0x30)) {
            const char* val = *reinterpret_cast<const char**>(reinterpret_cast<uint8_t*>(cv) + 0x28);
            PROBE_LOG("  original CVarGetString(\"probe_test\"): \"%s\" FloatBits=%08X",
                      val ? val : "(null)", FloatBits(static_cast<float>(atof(val ? val : "0"))));
        } else {
            PROBE_LOG("  original CVarGetString(\"probe_test\"): null (CVar not registered, expected)");
        }

        const char* fakeVal = *reinterpret_cast<const char**>(s_fakeCvar.data + 0x28);
        PROBE_LOG("  fake CVar value: \"%s\" FloatBits=%08X",
                  fakeVal ? fakeVal : "(null)", FloatBits(static_cast<float>(atof(fakeVal ? fakeVal : "0"))));

        auto* realCv = getFn("realmName");
        if (realCv && IsReadable(realCv, 0x30)) {
            const char* realVal = *reinterpret_cast<const char**>(reinterpret_cast<uint8_t*>(realCv) + 0x28);
            PROBE_LOG("  passthrough CVarGetString(\"realmName\"): \"%s\"", realVal ? realVal : "(null)");
        } else {
            PROBE_LOG("  passthrough CVarGetString(\"realmName\"): null");
        }
    } else {
        auto lookupFn = reinterpret_cast<uintptr_t* (__fastcall*)(const char*)>(0x0063DEC0);

        auto* cv = lookupFn("probe_test");
        if (cv && IsReadable(cv, 0x30)) {
            float val = *reinterpret_cast<float*>(reinterpret_cast<uint8_t*>(cv) + 0x24);
            PROBE_LOG("  original CVarLookup(\"probe_test\"): FloatBits=%08X", FloatBits(val));
        } else {
            PROBE_LOG("  original CVarLookup(\"probe_test\"): null (expected, not registered)");
        }

        float fakeVal = *reinterpret_cast<float*>(s_fakeCvar.data + 0x24);
        PROBE_LOG("  fake CVar value: FloatBits=%08X", FloatBits(fakeVal));

        auto* realCv = lookupFn("realmName");
        if (realCv && IsReadable(realCv, 0x30)) {
            float realVal = *reinterpret_cast<float*>(reinterpret_cast<uint8_t*>(realCv) + 0x24);
            PROBE_LOG("  passthrough CVarLookup(\"realmName\"): FloatBits=%08X", FloatBits(realVal));
        } else {
            PROBE_LOG("  passthrough CVarLookup(\"realmName\"): null");
        }
    }

    PROBE_LOG("=== CVar Intercept Test Complete ===");
}

static void ProbeModules() {
    PROBE_LOG("=== DLL Modules ===");

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
            bool isD3D9 = (_stricmp(name, "d3d9.dll") == 0);

            if (isD3D9) {
                char fullPath[MAX_PATH] = {};
                GetModuleFileNameA(modules[i], fullPath, MAX_PATH);
                PROBE_LOG("%-30s @ 0x%08X -- %s", name, (uintptr_t)modules[i], fullPath);
            } else {
                PROBE_LOG("%-30s @ 0x%08X", name, (uintptr_t)modules[i]);
            }
        }
    }

    PROBE_LOG("Total modules: %d", count);
}

static HRESULT __stdcall ProbeEndScene(IDirect3DDevice9* dev) {
    if (g_probed)
        return g_originalEndScene(dev);

    uintptr_t worldFrame = GetWorldFramePtr();
    if (!worldFrame)
        return g_originalEndScene(dev);

    g_probed = true;

    g_logFile = fopen("mods\\Probe.log", "a");
    if (!g_logFile)
        g_logFile = fopen("Probe.log", "a");

    PROBE_LOG("=== Deferred probes (world loaded) ===");
    PROBE_LOG("WorldFrame ptr: 0x%08X", worldFrame);

    InitFakeCVar();
    TestCVarIntercept();
    ProbeCamera();

    PROBE_LOG("=== Deferred probes complete ===");

    if (g_device) {
        void** vtable = *reinterpret_cast<void***>(g_device);
        DWORD oldProt;

        VirtualProtect(&vtable[VTABLE_ENDSCENE], sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProt);
        vtable[VTABLE_ENDSCENE] = reinterpret_cast<void*>(g_originalEndScene);
        VirtualProtect(&vtable[VTABLE_ENDSCENE], sizeof(void*), oldProt, &oldProt);

        VirtualProtect(&vtable[VTABLE_RESET], sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProt);
        vtable[VTABLE_RESET] = reinterpret_cast<void*>(g_originalReset);
        VirtualProtect(&vtable[VTABLE_RESET], sizeof(void*), oldProt, &oldProt);

        PROBE_LOG("vtable entries restored to original values");
    }

    HRESULT hr = g_originalEndScene(dev);

    fclose(g_logFile);
    g_logFile = nullptr;

    return hr;
}

static HRESULT __stdcall ProbeReset(IDirect3DDevice9* dev, D3DPRESENT_PARAMETERS* params) {
    return g_originalReset(dev, params);
}

static DWORD WINAPI InitThread(LPVOID) {
    g_logFile = fopen("mods\\Probe.log", "w");
    if (!g_logFile) {
        g_logFile = fopen("Probe.log", "w");
    }
    if (!g_logFile) return 1;

    PROBE_LOG("Probe.dll v0.1.0 starting");

    PROBE_LOG("=== Version Detection ===");
    DetectVersion();

    ValidateOffsets();
    ProbeD3DDevice();
    ProbeCamera();
    ProbeModules();

    PROBE_LOG("=== Complete (deferred probes will run when world loads) ===");

    fclose(g_logFile);
    g_logFile = nullptr;
    return 0;
}

static HMODULE g_hModule = NULL;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID) {
    switch (reason) {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hModule);
        g_hModule = hModule;
        CreateThread(NULL, 0, InitThread, NULL, 0, NULL);
        return TRUE;
    }
    return TRUE;
}
