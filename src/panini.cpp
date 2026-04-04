#include "panini.h"

float ReadCameraFov() {
    if (CVar_GetInt(CVar::Enabled, 0)) {
        float pf = CVar_GetFloat(CVar::Fov, kCVarFovDefault);
        if (IsValidFov(pf)) return pf;
    }

    float cvarFov = CVar_GetFloat(CVar::WowFov, 0.0f);
    if (IsValidFov(cvarFov)) return cvarFov;

    auto pCam = reinterpret_cast<uint8_t*>(g_ops.getCameraPtr());
    if (!pCam) return kPiOver2;

    float fov = *reinterpret_cast<float*>(pCam + g_offsets->Camera_FOV_Off);
    if (IsValidFov(fov)) return fov;

    return kPiOver2;
}

IDirect3DDevice9* GetWoWDevice() {
    auto pGx = *reinterpret_cast<uint8_t**>(g_offsets->CGxDeviceD3d_Ptr);
    if (!pGx) return nullptr;
    return *reinterpret_cast<IDirect3DDevice9**>(pGx + g_offsets->D3DDevice_Off);
}

static bool s_paniniWasEnabled = false;

void UpdateCameraFov() {
    bool enabled = CVar_GetInt(CVar::Enabled, 1) != 0;

    auto pCam = reinterpret_cast<uint8_t*>(g_ops.getCameraPtr());
    if (!pCam) return;

    if (enabled) {
        float pf = CVar_GetFloat(CVar::Fov, kCVarFovDefault);
        if (IsValidFov(pf))
            *reinterpret_cast<float*>(pCam + g_offsets->Camera_FOV_Off) = pf;
    } else if (s_paniniWasEnabled) {
        float fov = CVar_GetFloat(CVar::WowFov, kPiOver2);
        if (IsValidFov(fov))
            *reinterpret_cast<float*>(pCam + g_offsets->Camera_FOV_Off) = fov;
    }

    s_paniniWasEnabled = enabled;
}
