#include "panini.h"

float ReadCameraFov() {
    if (CVar_GetInt("paniniEnabled", 0)) {
        float pf = CVar_GetFloat("paniniFov", 0.0f);
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

float ReadCameraAspect() {
    auto pWF = *reinterpret_cast<uint8_t**>(wow::WorldFrame_Ptr);
    if (!pWF) return 1.7778f;
    auto pCam = *reinterpret_cast<uint8_t**>(pWF + wow::ActiveCamera_Off);
    if (!pCam) return 1.7778f;
    float a = *reinterpret_cast<float*>(pCam + wow::Camera_Aspect_Off);
    if (a <= 0.0f || a > 10.0f || a != a) return 1.7778f;
    return a;
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

static bool s_paniniWasEnabled = false;

void UpdateCameraFov() {
    bool enabled = CVar_GetInt("paniniEnabled", 0) != 0;

    auto pWF = *reinterpret_cast<uint8_t**>(wow::WorldFrame_Ptr);
    if (!pWF) return;
    auto pCam = *reinterpret_cast<uint8_t**>(pWF + wow::ActiveCamera_Off);
    if (!pCam) return;

#ifdef PANINI_DEBUG_LOG
    {
        static int s_ufDiag = 0;
        if (s_ufDiag++ < 2) {
            LOG_DEBUG("uf-diag", "pWF=%p pCam=%p fovAddr=%p enabled=%d",
                (void*)pWF, (void*)pCam,
                (void*)(pCam + wow::Camera_FOV_Off), enabled);
        }
    }
#endif

    if (enabled) {
        float pf = CVar_GetFloat("paniniFov", 0.0f);
        if (IsValidFov(pf))
            *reinterpret_cast<float*>(pCam + wow::Camera_FOV_Off) = pf;
    } else if (s_paniniWasEnabled) {
        float uf = CVar_GetFloat("paniniUserFov", 0.0f);
        if (IsValidFov(uf))
            *reinterpret_cast<float*>(pCam + wow::Camera_FOV_Off) = uf;
    }

    s_paniniWasEnabled = enabled;
}
