#include "panini.h"
#include "cvar_core.h"
#include "wow_offsets.h"
#include <cstdlib>

// WotLK CVar functions at the addresses in wow_offsets.h are __cdecl wrappers
// that load the CVarSystem singleton internally (MOV ECX, 0x00CA19FC). The caller
// pushes params and cleans the stack (confirmed by CVar__RegisterAll at 0x51D9B0:
// CALL 0x767FC0 followed by ADD ESP, 0x24).
typedef void* (__cdecl* CVarLookupWotlkFn)(const char* name);
typedef uint32_t (__cdecl* CVarRegisterWotlkFn)(
    char* name, char* description, uint32_t flags,
    char* defaultValue, void* callback, uint32_t category,
    uint8_t saveToWTF, uint32_t param_8, uint8_t param_9);

// Classic CVar system: standalone __fastcall functions.
typedef uintptr_t* (__fastcall* CVarLookupClassicFn)(const char* name);
typedef int* (__fastcall* CVarRegisterClassicFn)(
    char* name, char* help,
    int flags, const char* defaultValue,
    void* callback, int category,
    char setCommand, int arg
);

typedef void* (__cdecl* GetActiveCameraFn)();

static CVarLookupClassicFn   s_classicLookup = nullptr;
static CVarRegisterClassicFn s_classicReg    = nullptr;
static GetActiveCameraFn     s_getActiveCamera = nullptr;

// ---------------------------------------------------------------------------
// WotLK implementations
// ---------------------------------------------------------------------------

static void wotlk_registerCVar(const char* name, const char* defaultVal) {
    if (!g_offsets->CVarRegister) return;
    auto regFn = reinterpret_cast<CVarRegisterWotlkFn>(g_offsets->CVarRegister);
    regFn((char*)name, nullptr, 0, (char*)defaultVal, nullptr, 5, 0, 0, 0);
}

// WotLK CVar struct: string value at +0x28 (confirmed by CVarGetString wrapper
// analysis and probe v5). Parse string to avoid struct offset uncertainty in
// float/int fields.
static float wotlk_getCVarFloat(const char* name, float fallback) {
    if (!g_offsets->CVarLookup) return fallback;
    auto lookupFn = reinterpret_cast<CVarLookupWotlkFn>(g_offsets->CVarLookup);
    auto cv = lookupFn(name);
    if (!cv) return fallback;
    auto str = *reinterpret_cast<const char**>(
        reinterpret_cast<uint8_t*>(cv) + 0x28);
    if (!str || !str[0]) return fallback;
    char* end = nullptr;
    float v = strtof(str, &end);
    if (end == str) return fallback;
    return (v == v) ? v : fallback;
}

static int wotlk_getCVarInt(const char* name, int fallback) {
    if (!g_offsets->CVarLookup) return fallback;
    auto lookupFn = reinterpret_cast<CVarLookupWotlkFn>(g_offsets->CVarLookup);
    auto cv = lookupFn(name);
    if (!cv) return fallback;
    auto str = *reinterpret_cast<const char**>(
        reinterpret_cast<uint8_t*>(cv) + 0x28);
    if (!str || !str[0]) return fallback;
    char* end = nullptr;
    long v = strtol(str, &end, 10);
    if (end == str) return fallback;
    return static_cast<int>(v);
}

static void* wotlk_getCameraPtr() {
    return s_getActiveCamera ? s_getActiveCamera() : nullptr;
}

static bool wotlk_isWorldActive() {
    return s_getActiveCamera && s_getActiveCamera() != nullptr;
}

// RenderWorld at 0x4FAF90 is __cdecl with WorldFrame* as the first stack arg.
// The trampoline must receive the same argument to set up a correct stack frame.
static void wotlk_callOriginalRenderWorld(uintptr_t target, void* worldFrame) {
    reinterpret_cast<void(__cdecl*)(void*)>(target)(worldFrame);
}

// ---------------------------------------------------------------------------
// Classic implementations
// ---------------------------------------------------------------------------

static void classic_registerCVar(const char* name, const char* defaultVal) {
    s_classicReg((char*)name, nullptr, 0, defaultVal, nullptr, 5, 0, 0);
}

static float classic_getCVarFloat(const char* name, float fallback) {
    auto cv = s_classicLookup(name);
    if (!cv) return fallback;
    auto base = reinterpret_cast<uint8_t*>(cv);
    float v = *reinterpret_cast<float*>(base + g_offsets->CVar_FloatValue);
    return (v == v) ? v : fallback;
}

static int classic_getCVarInt(const char* name, int fallback) {
    auto cv = s_classicLookup(name);
    if (!cv) return fallback;
    return *reinterpret_cast<int*>(
        reinterpret_cast<uint8_t*>(cv) + g_offsets->CVar_IntValue);
}

static void* classic_getCameraPtr() {
    auto pWF = *reinterpret_cast<uint8_t**>(g_offsets->WorldFrame_Ptr);
    if (!pWF) return nullptr;
    return *reinterpret_cast<uint8_t**>(pWF + g_offsets->ActiveCamera_Off);
}

static bool classic_isWorldActive() {
    auto pWF = *reinterpret_cast<uint8_t**>(g_offsets->WorldFrame_Ptr);
    return pWF != nullptr;
}

static void classic_callOriginalRenderWorld(uintptr_t target, void* ecx) {
    __asm__ __volatile__ (
        "mov %0, %%ecx\n\t"
        "call *%1\n\t"
        :
        : "r"(ecx), "r"(target)
        : "eax", "edx", "ecx", "memory"
    );
}

// ---------------------------------------------------------------------------
// InitVersionOps
// ---------------------------------------------------------------------------

VersionOps g_ops = {};

void InitVersionOps() {
    if (g_offsets->version == WowVersion::WotLK335) {
        g_ops.registerCVar           = wotlk_registerCVar;
        g_ops.getCVarFloat           = wotlk_getCVarFloat;
        g_ops.getCVarInt             = wotlk_getCVarInt;
        g_ops.getCameraPtr           = wotlk_getCameraPtr;
        g_ops.isWorldActive          = wotlk_isWorldActive;
        g_ops.callOriginalRenderWorld = wotlk_callOriginalRenderWorld;
        g_ops.renderWorldPatchSize   = 9;

        if (g_offsets->GetActiveCamera_Addr) {
            s_getActiveCamera = reinterpret_cast<GetActiveCameraFn>(
                g_offsets->GetActiveCamera_Addr);
        }
    } else {
        s_classicLookup = reinterpret_cast<CVarLookupClassicFn>(g_offsets->CVarLookup);
        s_classicReg    = reinterpret_cast<CVarRegisterClassicFn>(g_offsets->CVarRegister);

        g_ops.registerCVar           = classic_registerCVar;
        g_ops.getCVarFloat           = classic_getCVarFloat;
        g_ops.getCVarInt             = classic_getCVarInt;
        g_ops.getCameraPtr           = classic_getCameraPtr;
        g_ops.isWorldActive          = classic_isWorldActive;
        g_ops.callOriginalRenderWorld = classic_callOriginalRenderWorld;
        g_ops.renderWorldPatchSize   = 9;
    }
}
