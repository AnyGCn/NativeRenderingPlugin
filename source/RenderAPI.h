#pragma once

#include <stddef.h>
#include <stdint.h>

#include "Unity/IUnityRenderingExtensions.h"
#include "Unity/IUnityProfiler.h"
#include "Unity/IUnityLog.h"

struct IUnityInterfaces;

struct TopLevelAccelerationStructureElementDescriptor
{
    int meshIndex;
    float transformMatrix[16];
};

struct BottomLevelAccelerationStructureDescriptor
{
    void* positionBuffer;
    int vertexBufferOffset;
    int vertexFormat;
    int vertexStride;
    void* indexBuffer;
    int indexBufferOffset;
    int indexType;
    int indexCount;
};

struct CameraData
{
    //! Specifies matrix transformation from the camera view to the clip space.
    float cameraViewToClip[16];
    //! Specifies matrix transformation from the clip space to the camera view space.
    float clipToCameraView[16];
    //! Optional - Specifies matrix transformation describing lens distortion in clip space.
    // float clipToLensClip[16];
    //! Specifies matrix transformation from the current clip to the previous clip space.
    //! clipToPrevClip = clipToView * viewToViewPrev * viewToClipPrev
    //! Sample code can be found in sl_matrix_helpers.h
    float clipToPrevClip[16];
    //! Specifies matrix transformation from the previous clip to the current clip space.
    //! prevClipToClip = clipToPrevClip.inverse()
    float prevClipToClip[16];

    //! Specifies pixel space jitter offset
    float jitterOffset[2];
    //! Specifies scale factors used to normalize motion vectors (so the values are in [-1,1] range)
    float mvecScale[2];
    //! Optional - Specifies camera pinhole offset if used.
    // float cameraPinholeOffset[2];
    //! Specifies camera position in world space.
    float cameraPos[3];
    //! Specifies camera up vector in world space.
    float cameraUp[3];
    //! Specifies camera right vector in world space.
    float cameraRight[3];
    //! Specifies camera forward vector in world space.
    float cameraFwd[3];

    //! Specifies camera near view plane distance.
    float cameraNear;
    //! Specifies camera far view plane distance.
    float cameraFar;
    //! Specifies camera field of view in radians.
    float cameraFOV;
    //! Specifies camera aspect ratio defined as view space width divided by height.
    // float cameraAspectRatio;
    //! Specifies which value represents an invalid (un-initialized) value in the motion vectors buffer
    //! NOTE: This is only required if `cameraMotionIncluded` is set to false and SL needs to compute it.
    // float motionVectorsInvalidValue;

	int viewHandle = 0;

	//! Specifies input texture size
	int inputSize[2];

	//! Specifies output texture size
	int outputSize[2];

    //! Specifies if depth values are inverted (value closer to the camera is higher) or not.
    bool depthInverted = true;
    //! Specifies if camera motion is included in the MVec buffer.
    // bool cameraMotionIncluded = true;
    //! Specifies if motion vectors are 3D or not.
    // bool motionVectors3D = false;
    //! Specifies if previous frame has no connection to the current one (i.e. motion vectors are invalid)
    bool reset = false;
    //! Specifies if orthographic projection is used or not.
    // bool orthographicProjection = false;
    //! Specifies if motion vectors are already dilated or not.
    // bool motionVectorsDilated = false;
    //! Specifies if motion vectors are jittered or not.
    // bool motionVectorsJittered = true;

	bool colorBuffersHDR = true;

	bool alphaUpscalingEnabled = false;
};

enum DLSSMode
{
	eOff,
	eMaxPerformance,
	eBalanced,
	eMaxQuality,
	eUltraPerformance,
	eUltraQuality,
	eDLAA,
	eDLSSModeCount,
};

enum TextureType
{
	eDepth,
	eMotionVectors,
	eHUDLessColor,
	eScalingInputColor,
	eScalingOutputColor,
    eNormal,
    eRaytracingOutput,
	eTextureTypeCount,
};

// Plugin event IDs passed to IssuePluginEvent / IssuePluginEventAndData from the Unity side.
// Must stay in sync with the C# enum on the Unity side.
enum RenderEventType
{
    Sync_RenderStart,
    Sync_RenderEnd,
    Upscale_DLSS,
    Cleanup_DLSS,
    Upscale_MetalFX_Spatial,
    Upscale_MetalFX_Temporal,
    Cleanup_MetalFX,
    Dispatch_Raytracing,
    Cleanup_Raytracing,
    RenderEventCount,
};

struct DLSSSettings
{
	int optimalRenderSizeX;
	int optimalRenderSizeY;
	int minRenderSizeX;
	int minRenderSizeY;
	int maxRenderSizeX;
	int maxRenderSizeY;
	float sharpness;
};

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
    virtual void SetBlasDescriptors(const BottomLevelAccelerationStructureDescriptor* blasDescriptors, const int* pSubmeshCount, int count) {}
    virtual void SetTlasDescriptors(const TopLevelAccelerationStructureElementDescriptor* tlasDescriptor, int count) {}
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
