#include "picking.h"
#include "log.h"
#include <windows.h>
#include <cstdint>
#include <cstring>

namespace {
constexpr uintptr_t kImageBase = 0x00400000;
constexpr uintptr_t kRayCall = 0x0048130C;
constexpr uintptr_t kScreenToRay = 0x004813B0;
constexpr uintptr_t kWorldFrame = 0x00B4B2BC;
constexpr uintptr_t kDeviceWidth = 0x00832A44;
constexpr uintptr_t kDeviceHeight = 0x00832A48;
constexpr size_t kCameraOffset = 0x65B8;
using ScreenToRay = int (__thiscall*)(void*, float, float, void*, void*);

struct Snapshot {
    PickingProjection projection;
    void* worldFrame = nullptr;
    void* camera = nullptr;
    float width = 0.0f;
    float height = 0.0f;
};

SRWLOCK g_snapshotLock = SRWLOCK_INIT;
Snapshot g_snapshot;
bool g_installed = false;

uint32_t Fingerprint(const uint8_t* data, size_t size) {
    uint32_t hash = 2166136261u;
    for (size_t i = 0; i < size; ++i) hash = (hash ^ data[i]) * 16777619u;
    return hash;
}

// __fastcall supplies ECX=this and an unused EDX, preserving the original four
// stack arguments and ret 16. Only the world hit-test's CALL is redirected.
int __fastcall PickRay(void* worldFrame, void*, float x, float y, void* nearRay, void* farRay) {
    AcquireSRWLockShared(&g_snapshotLock);
    Snapshot snapshot = g_snapshot;
    ReleaseSRWLockShared(&g_snapshotLock);

    if (snapshot.worldFrame == worldFrame && worldFrame &&
        *reinterpret_cast<void**>(kWorldFrame) == worldFrame &&
        *reinterpret_cast<void**>(static_cast<uint8_t*>(worldFrame) + kCameraOffset) == snapshot.camera &&
        *reinterpret_cast<float*>(kDeviceWidth) == snapshot.width &&
        *reinterpret_cast<float*>(kDeviceHeight) == snapshot.height) {
        if (!RemapPickingPoint(snapshot.projection, snapshot.width, snapshot.height, x, y))
            return 0;
    }

    return reinterpret_cast<ScreenToRay>(kScreenToRay)(worldFrame, x, y, nearRay, farRay);
}
} // namespace

bool Picking_Install(void* clientImage) {
    if (g_installed) return true;
    if (reinterpret_cast<uintptr_t>(clientImage) != kImageBase) return false;
    auto image = static_cast<uint8_t*>(clientImage);
    auto dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(image);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew < 0 || dos->e_lfanew > 0x1000)
        return false;
    auto nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>(image + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE || nt->FileHeader.Machine != IMAGE_FILE_MACHINE_I386 ||
        nt->FileHeader.TimeDateStamp != 0x4510B6DB ||
        nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC ||
        nt->OptionalHeader.ImageBase != kImageBase ||
        nt->OptionalHeader.SizeOfImage < kWorldFrame + sizeof(void*) - kImageBase) return false;

    auto call = reinterpret_cast<uint8_t*>(kRayCall);
    const uint8_t expected[] = {0xE8, 0x9F, 0x00, 0x00, 0x00, 0x85, 0xC0, 0x74, 0x73};
    // Hash the entire ray function, including bounds checks, camera offset and
    // stack cleanup. Refuse other mods' hooks instead of guessing their ABI.
    if (memcmp(call, expected, sizeof(expected)) != 0 ||
        Fingerprint(reinterpret_cast<const uint8_t*>(kScreenToRay), 0x105) != 0x02683183u) {
        LOG_INFO("pick", "Classic code mismatch; picking correction not installed");
        return false;
    }

    DWORD oldProtection;
    if (!VirtualProtect(call, 5, PAGE_EXECUTE_READWRITE, &oldProtection)) return false;
    int32_t displacement = static_cast<int32_t>(
        reinterpret_cast<uintptr_t>(&PickRay) - (kRayCall + 5));
    memcpy(call + 1, &displacement, sizeof(displacement));
    DWORD ignored;
    if (!FlushInstructionCache(GetCurrentProcess(), call, 5)) {
        memcpy(call, expected, 5);
        FlushInstructionCache(GetCurrentProcess(), call, 5);
        VirtualProtect(call, 5, oldProtection, &ignored);
        return false;
    }
    if (!VirtualProtect(call, 5, oldProtection, &ignored))
        LOG_INFO("pick", "could not restore call-site memory protection");
    g_installed = true;
    LOG_INFO("pick", "Classic world picking installed at 0x%08X", unsigned(kRayCall));
    return true;
}

void Picking_Publish(void* worldFrame, const PickingProjection& projection) {
    if (!g_installed || !worldFrame) return;
    Snapshot snapshot;
    snapshot.projection = projection;
    snapshot.worldFrame = worldFrame;
    snapshot.camera = *reinterpret_cast<void**>(static_cast<uint8_t*>(worldFrame) + kCameraOffset);
    snapshot.width = *reinterpret_cast<float*>(kDeviceWidth);
    snapshot.height = *reinterpret_cast<float*>(kDeviceHeight);
    // A resize must produce a new rendered snapshot before it affects picking.
    if (!snapshot.camera || !std::isfinite(snapshot.width) || !std::isfinite(snapshot.height) ||
        snapshot.width <= 0.0f || snapshot.height <= 0.0f ||
        fabsf(snapshot.width / snapshot.height - projection.aspect) > 0.0001f)
        snapshot = {};
    AcquireSRWLockExclusive(&g_snapshotLock);
    g_snapshot = snapshot;
    ReleaseSRWLockExclusive(&g_snapshotLock);
}

void Picking_Invalidate() {
    AcquireSRWLockExclusive(&g_snapshotLock);
    g_snapshot = {};
    ReleaseSRWLockExclusive(&g_snapshotLock);
}
