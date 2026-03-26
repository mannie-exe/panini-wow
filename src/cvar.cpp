#include "panini.h"
#include <cstring>

typedef uintptr_t* (__fastcall* CVarLookup_t)(const char* name);
typedef int* (__fastcall* CVarRegister_t)(
    char* name, char* help,
    int flags, const char* defaultValue,
    void* callback, int category,
    char setCommand, int arg
);

static const auto CVarLookup   = reinterpret_cast<CVarLookup_t>(wow::CVarLookup_Addr);
static const auto CVarRegister = reinterpret_cast<CVarRegister_t>(wow::CVarRegister_Addr);

void CVar_RegisterAll() {
    char n1[] = "paniniEnabled";
    char n2[] = "paniniStrength";
    char n3[] = "paniniVertComp";
    char n4[] = "paniniFill";
    char n5[] = "paniniUserFov";
    char n6[] = "paniniDebug";
    char n7[] = "paniniFov";

    CVarRegister(n1, nullptr, 0, "0",   nullptr, 5, 0, 0);
    CVarRegister(n2, nullptr, 0, "0.5", nullptr, 5, 0, 0);
    CVarRegister(n3, nullptr, 0, "0.0", nullptr, 5, 0, 0);
    CVarRegister(n4, nullptr, 0, "0.8", nullptr, 5, 0, 0);
    CVarRegister(n5, nullptr, 0, "0",   nullptr, 5, 0, 0);
    CVarRegister(n7, nullptr, 0, "2.0", nullptr, 5, 0, 0);
    CVarRegister(n6, nullptr, 0, "0",   nullptr, 5, 0, 0);

    LogInfo("cvar", "registered 7 CVars");
}

// Read the cached float at +0x24. Verified via runtime hex dump:
// 0x3FC90FDA = 1.5707963f. SetCVar DOES update this field.
float CVar_GetFloat(const char* name, float fallback) {
    auto cv = CVarLookup(name);
    if (!cv) return fallback;
    float v = *reinterpret_cast<float*>(reinterpret_cast<uint8_t*>(cv) + wow::CVar_FloatValue);
    if (v != v) return fallback; // NaN guard
    return v;
}

// Read the cached int at +0x28.
int CVar_GetInt(const char* name, int fallback) {
    auto cv = CVarLookup(name);
    if (!cv) return fallback;
    return *reinterpret_cast<int*>(reinterpret_cast<uint8_t*>(cv) + wow::CVar_IntValue);
}

static bool g_cvarLogged = false;

void PaniniConfig_ReadFromCVars(PaniniConfig* cfg) {
    cfg->enabled      = CVar_GetInt("paniniEnabled", 0) != 0;
    cfg->strength     = CVar_GetFloat("paniniStrength", 0.5f);
    cfg->verticalComp = CVar_GetFloat("paniniVertComp", 0.0f);
    cfg->fill         = CVar_GetFloat("paniniFill", 0.8f);
    cfg->debug        = CVar_GetInt("paniniDebug", 0) != 0;

    if ((cfg->enabled || cfg->debug) && !g_cvarLogged) {
        // Cast to double for varargs; MinGW 32-bit printf promotes float incorrectly
        LogInfo("cvar", "active: enabled=%d debug=%d D=%.3f S=%.3f fill=%.3f",
                cfg->enabled, cfg->debug,
                (double)cfg->strength, (double)cfg->verticalComp, (double)cfg->fill);
        g_cvarLogged = true;
    }
}
