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
    char n5[] = "paniniDebugTint";
    char n6[] = "paniniDebugUV";
    char n7[] = "paniniFov";
    char n8[] = "paniniUserFov";

    CVarRegister(n1, nullptr, 0, "0",      nullptr, 5, 0, 0);
    CVarRegister(n2, nullptr, 0, "0.0333", nullptr, 5, 0, 0);
    CVarRegister(n3, nullptr, 0, "0.0",    nullptr, 5, 0, 0);
    CVarRegister(n4, nullptr, 0, "1.0",    nullptr, 5, 0, 0);
    CVarRegister(n5, nullptr, 0, "0",      nullptr, 5, 0, 0);
    CVarRegister(n6, nullptr, 0, "0",      nullptr, 5, 0, 0);
    CVarRegister(n7, nullptr, 0, "2.6",    nullptr, 5, 0, 0);
    CVarRegister(n8, nullptr, 0, "-1",     nullptr, 5, 0, 0);

    LOG_INFO("cvar", "registered 8 CVars");
}

float CVar_GetFloat(const char* name, float fallback) {
    auto cv = CVarLookup(name);
    if (!cv) return fallback;
    auto base = reinterpret_cast<uint8_t*>(cv);
    float v = *reinterpret_cast<float*>(base + wow::CVar_FloatValue);
    if (v != v) return fallback;
    return v;
}

int CVar_GetInt(const char* name, int fallback) {
    auto cv = CVarLookup(name);
    if (!cv) return fallback;
    return *reinterpret_cast<int*>(reinterpret_cast<uint8_t*>(cv) + wow::CVar_IntValue);
}

void PaniniConfig_ReadFromCVars(PaniniConfig* cfg) {
    cfg->enabled      = CVar_GetInt("paniniEnabled", 0) != 0;
    cfg->strength     = CVar_GetFloat("paniniStrength", 0.0333f);
    cfg->verticalComp = CVar_GetFloat("paniniVertComp", 0.0f);
    cfg->fill         = CVar_GetFloat("paniniFill", 1.0f);
    cfg->debugTint    = CVar_GetInt("paniniDebugTint", 0) != 0;
    cfg->debugUV      = CVar_GetInt("paniniDebugUV", 0) != 0;
}
