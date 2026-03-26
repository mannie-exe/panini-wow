#include "panini.h"
#include <cmath>

float ComputeFillZoom(float strength, float halfTanFov, float aspect, float fill) {
    if (strength < 0.001f || fill < 0.001f)
        return 1.0f;

    float lonMax = atanf(halfTanFov * aspect);
    float denom  = strength + cosf(lonMax);
    if (denom < 0.001f)
        return 1.0f;

    float k     = (strength + 1.0f) / denom;
    float edgeX = k * sinf(lonMax);
    float fitX  = fabsf(edgeX) / (halfTanFov * aspect);
    if (fitX < 0.001f)
        return 1.0f;

    float zoom = 1.0f / fitX;
    return 1.0f + (zoom - 1.0f) * fill;
}

static inline bool IsValidFov(float f) {
    return f > 0.05f && f < 3.5f && f == f;
}

static uint8_t* GetCameraPtr() {
    auto pWF = *reinterpret_cast<uint8_t**>(wow::WorldFrame_Ptr);
    if (!pWF) return nullptr;
    return *reinterpret_cast<uint8_t**>(pWF + wow::ActiveCamera_Off);
}

static bool g_cameraLogged = false;

float ReadCameraFov() {
    auto pCam = GetCameraPtr();
    if (pCam) {
        float fov = *reinterpret_cast<float*>(pCam + wow::Camera_FOV_Off);
        if (!g_cameraLogged) {
            LogInfo("camera", "ptr=%p fov=%.4f (0x%08X)", pCam, (double)fov,
                    *reinterpret_cast<uint32_t*>(pCam + wow::Camera_FOV_Off));
            g_cameraLogged = true;
        }
        if (IsValidFov(fov)) return fov;
    }

    float cvarFov = CVar_GetFloat("FoV", 0.0f);
    if (IsValidFov(cvarFov)) return cvarFov;

    return 1.5708f;
}

float ReadCameraAspect() {
    auto pCam = GetCameraPtr();
    if (!pCam) return 1.7778f;
    float a = *reinterpret_cast<float*>(pCam + wow::Camera_Aspect_Off);
    if (a <= 0.0f || a > 10.0f || a != a) return 1.7778f;
    return a;
}

void WriteCameraFov(float fov) {
    auto pCam = GetCameraPtr();
    if (pCam && IsValidFov(fov))
        *reinterpret_cast<float*>(pCam + wow::Camera_FOV_Off) = fov;
}

IDirect3DDevice9* GetWoWDevice() {
    auto pGx = *reinterpret_cast<uint8_t**>(wow::CGxDeviceD3d_Ptr);
    if (!pGx) return nullptr;
    return *reinterpret_cast<IDirect3DDevice9**>(pGx + wow::D3DDevice_Off);
}
