#include "panini.h"

float ReadCameraFov() {
    if (CVar_GetInt("paniniEnabled", 0)) {
        float pf = CVar_GetFloat("paniniFov", 2.82f);
        if (IsValidFov(pf)) return pf;
    }

    float cvarFov = CVar_GetFloat("FoV", 0.0f);
    if (IsValidFov(cvarFov)) return cvarFov;

    auto pWF = *reinterpret_cast<uint8_t**>(wow::WorldFrame_Ptr);
    if (!pWF) return 1.5708f;
    auto pCam = *reinterpret_cast<uint8_t**>(pWF + wow::ActiveCamera_Off);
    if (!pCam) return 1.5708f;

    float fov = *reinterpret_cast<float*>(pCam + wow::Camera_FOV_Off);
    if (IsValidFov(fov)) return fov;

    return 1.5708f;
}

IDirect3DDevice9* GetWoWDevice() {
    auto pGx = *reinterpret_cast<uint8_t**>(wow::CGxDeviceD3d_Ptr);
    if (!pGx) return nullptr;
    return *reinterpret_cast<IDirect3DDevice9**>(pGx + wow::D3DDevice_Off);
}

bool IsWorldActive() {
    auto pWF = *reinterpret_cast<uint8_t**>(wow::WorldFrame_Ptr);
    return pWF != nullptr;
}

void UpdateCameraFov() {
    if (CVar_GetInt("paniniEnabled", 1) == 0) return;

    auto pWF = *reinterpret_cast<uint8_t**>(wow::WorldFrame_Ptr);
    if (!pWF) return;
    auto pCam = *reinterpret_cast<uint8_t**>(pWF + wow::ActiveCamera_Off);
    if (!pCam) return;

    float pf = CVar_GetFloat("paniniFov", 2.82f);
    if (IsValidFov(pf))
        *reinterpret_cast<float*>(pCam + wow::Camera_FOV_Off) = pf;
}
