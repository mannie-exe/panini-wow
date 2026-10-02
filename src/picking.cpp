#include "picking.h"
#include "log.h"
#include <windows.h>
#include <cstdint>
#include <cstring>

namespace {
constexpr uintptr_t kImageBase = 0x00400000;
struct PickingClient {
    const char* name;
    uint32_t timestamp;
    uintptr_t rayCall;
    uintptr_t screenToRay;
    uintptr_t worldFrame;
    uintptr_t deviceWidth;
    uintptr_t deviceHeight;
    size_t cameraOffset;
    size_t raySize;
    uint32_t rayFingerprint;
    uint8_t expectedCall[9];
};

constexpr PickingClient kClients[] = {
    {"Classic 5875", 0x4510B6DB, 0x0048130C, 0x004813B0, 0x00B4B2BC,
     0x00832A44, 0x00832A48, 0x65B8, 0x105, 0x02683183,
     {0xE8, 0x9F, 0x00, 0x00, 0x00, 0x85, 0xC0, 0x74, 0x73}},
    {"WotLK 12340", 0x4C2452FE, 0x004F9EC1, 0x004F6450, 0x00B7436C,
     0x00AC0CB4, 0x00AC0CB8, 0x7E20, 0x109, 0xA4015675,
     {0xE8, 0x8A, 0xC5, 0xFF, 0xFF, 0x85, 0xC0, 0x74, 0x6B}},
};
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
const PickingClient* g_client = nullptr;

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
        *reinterpret_cast<void**>(g_client->worldFrame) == worldFrame &&
        *reinterpret_cast<void**>(static_cast<uint8_t*>(worldFrame) + g_client->cameraOffset) == snapshot.camera &&
        *reinterpret_cast<float*>(g_client->deviceWidth) == snapshot.width &&
        *reinterpret_cast<float*>(g_client->deviceHeight) == snapshot.height) {
        if (!RemapPickingPoint(snapshot.projection, snapshot.width, snapshot.height, x, y))
            return 0;
    }

    return reinterpret_cast<ScreenToRay>(g_client->screenToRay)(worldFrame, x, y, nearRay, farRay);
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
        nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC ||
        nt->OptionalHeader.ImageBase != kImageBase) return false;

    const PickingClient* client = nullptr;
    for (const auto& candidate : kClients) {
        if (candidate.timestamp == nt->FileHeader.TimeDateStamp) client = &candidate;
    }
    if (!client || nt->OptionalHeader.SizeOfImage < client->worldFrame + sizeof(void*) - kImageBase)
        return false;

    auto call = reinterpret_cast<uint8_t*>(client->rayCall);
    // Hash the entire ray function, including bounds checks, camera offset and
    // stack cleanup. Refuse other mods' hooks instead of guessing their ABI.
    if (memcmp(call, client->expectedCall, sizeof(client->expectedCall)) != 0 ||
        Fingerprint(reinterpret_cast<const uint8_t*>(client->screenToRay), client->raySize) != client->rayFingerprint) {
        LOG_INFO("pick", "%s code mismatch; picking correction not installed", client->name);
        return false;
    }

    DWORD oldProtection;
    if (!VirtualProtect(call, 5, PAGE_EXECUTE_READWRITE, &oldProtection)) return false;
    int32_t displacement = static_cast<int32_t>(
        reinterpret_cast<uintptr_t>(&PickRay) - (client->rayCall + 5));
    g_client = client;
    memcpy(call + 1, &displacement, sizeof(displacement));
    DWORD ignored;
    if (!FlushInstructionCache(GetCurrentProcess(), call, 5)) {
        memcpy(call, client->expectedCall, 5);
        FlushInstructionCache(GetCurrentProcess(), call, 5);
        VirtualProtect(call, 5, oldProtection, &ignored);
        g_client = nullptr;
        return false;
    }
    if (!VirtualProtect(call, 5, oldProtection, &ignored))
        LOG_INFO("pick", "could not restore call-site memory protection");
    g_installed = true;
    LOG_INFO("pick", "%s world picking installed at 0x%08X", client->name, unsigned(client->rayCall));
    return true;
}

void Picking_Publish(void* worldFrame, const PickingProjection& projection) {
    if (!g_installed || !worldFrame) return;
    Snapshot snapshot;
    snapshot.projection = projection;
    snapshot.worldFrame = worldFrame;
    snapshot.camera = *reinterpret_cast<void**>(static_cast<uint8_t*>(worldFrame) + g_client->cameraOffset);
    snapshot.width = *reinterpret_cast<float*>(g_client->deviceWidth);
    snapshot.height = *reinterpret_cast<float*>(g_client->deviceHeight);
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
