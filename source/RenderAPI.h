#pragma once

#include <stddef.h>
#include <stdint.h>

#include "Unity/IUnityRenderingExtensions.h"
#include "Unity/IUnityProfiler.h"
#include "Unity/IUnityLog.h"
#include "RenderStructures.h"

struct IUnityInterfaces;

// Super-simple "graphics abstraction". This is nothing like how a proper platform abstraction layer would look like;
// all this does is a base interface for whatever our plugin sample needs. Which is only "draw some triangles"
// and "modify a texture" at this point.
//
// There are implementations of this base class for D3D9, D3D11, OpenGL etc.; see individual RenderAPI_* files.
class RenderAPI
{
protected:
    void* m_Textures[TextureType::eTextureTypeCount];
    CameraData m_CameraData;

public:
    virtual ~RenderAPI() { }

    // --------------------------------------------------------------------------
    // General plugin functions
    // --------------------------------------------------------------------------
    virtual void ProcessDeviceEvent(UnityGfxDeviceEventType type, IUnityInterfaces* interfaces) {};
    virtual bool ProcessRenderingExtQuery(UnityRenderingExtQueryType query) { return false; }
    virtual void SetCameraData(void* data) { m_CameraData = *static_cast<CameraData*>(data); }
    virtual void SetTexture(TextureType type, void* nativeTexture) { m_Textures[type] = nativeTexture; }

    // --------------------------------------------------------------------------
    // Metal plugin specific functions
    // --------------------------------------------------------------------------
    virtual bool SupportMetalFX() { return false; }
    virtual void UpscaleTextureMetalFXSpatial() {}
    virtual void UpscaleTextureMetalFXTemporal() {}
    virtual void CleanupMetalFX() {}
    virtual void SetRaytracingInstances(const InstanceDescriptor* pInstances, int count) {}
    virtual void SetRaytracingLights(const LightDescriptor* pLights, int count) {}
    virtual void SetRaytracingMaterials(const MaterialDscriptor* pMaterials, int count) {}
    virtual void SetRaytracingMeshes(const MeshDescriptor* pMeshes, int count) {}
    virtual bool SupportRaytracing() { return false; }
    virtual void DispatchRaytracing() {}
    virtual void CleanupRaytracing() {}

    // --------------------------------------------------------------------------
    // NVIDIA plugin specific functions
    // --------------------------------------------------------------------------
    virtual bool SupportDLSS() { return false; }
    virtual void CleanupDLSS() {}
    virtual void SetDLSSOptions(DLSSMode mode) {}
    virtual DLSSSettings QueryDLSSOptimalSettings(int outputSizeX, int outputSizeY, DLSSMode mode) { return DLSSSettings{}; }
    virtual void UpscaleTextureDLSS() {}
    virtual void ReflexCallback_Sleep(uint32_t frameID) {}
    virtual void ReflexCallback_SimStart(uint32_t frameID) {}
    virtual void ReflexCallback_SimEnd(uint32_t frameID) {}
    virtual void ReflexCallback_RenderStart(uint32_t frameID) {}
    virtual void ReflexCallback_RenderEnd(uint32_t frameID) {}

    virtual bool SupportFrameExtrapolate() { return false; }
    virtual void FrameExtrapolate(void* data) {}

    static IUnityLog* s_Logger;
    static void LogInfo(const char* fmt...);
    static void LogWarning(const char* fmt...);
    static void LogError(const char* fmt...);
    static void LogFatal(const char* fmt...);
    static IUnityProfiler* s_UnityProfiler;
    static const UnityProfilerMarkerDesc* s_ProfilerPresentMarker;
};

// Create a graphics API implementation instance for the given API type.
RenderAPI* CreateRenderAPI(UnityGfxRenderer apiType);
