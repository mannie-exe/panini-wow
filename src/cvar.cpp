#include "panini.h"
#include "cvar_core.h"
#include "version.g.h"

void CVar_RegisterAll() {
    for (int i = 0; i < (int)CVar::Version; ++i) {
        const char* name = kCVarTable[i].name;
        const char* def = kCVarTable[i].defaultVal;
        g_ops.registerCVar(name, def);
    }
    g_ops.registerCVar(kCVarTable[(int)CVar::Version].name, PANINI_VERSION);
    LOG_INFO("cvar", "registered %d CVars", (int)CVar::Version + 1);
}

float CVar_GetFloat(CVar id, float fallback) {
    return g_ops.getCVarFloat(kCVarTable[(int)id].name, fallback);
}

int CVar_GetInt(CVar id, int fallback) {
    return g_ops.getCVarInt(kCVarTable[(int)id].name, fallback);
}

void PostProcessConfig_ReadFromCVars(PostProcessConfig* cfg) {
    cfg->paniniEnabled = CVar_GetInt(CVar::Enabled, 1) != 0;
    cfg->strength      = CVar_GetFloat(CVar::Strength, 0.01f);
    cfg->verticalComp  = CVar_GetFloat(CVar::VertComp, 0.0f);
    cfg->fill          = CVar_GetFloat(CVar::Fill, 1.0f);
    cfg->fxaaEnabled   = CVar_GetInt(CVar::Fxaa, 1) != 0;
    cfg->sharpen       = CVar_GetFloat(CVar::Sharpen, 0.2f);
    cfg->debugTint     = CVar_GetInt(CVar::DebugTint, 0) != 0;
    cfg->debugUV       = CVar_GetInt(CVar::DebugUV, 0) != 0;
}
