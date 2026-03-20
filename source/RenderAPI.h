#pragma once

#include "Unity/IUnityRenderingExtensions.h"
#include "Unity/IUnityProfiler.h"
#include "Unity/IUnityLog.h"

#include <stddef.h>
#include <stdint.h>

struct IUnityInterfaces;

struct CameraData
{
    //! Specifies matrix transformation from the camera view to the clip space.
    float cameraViewToClip[16];
    //! Specifies matrix transformation from the clip space to the camera view space.
    float clipToCameraView[16];
    //! Optional - Specifies matrix transformation describing lens distortion in clip space.
    float clipToLensClip[16];
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
    float cameraPinholeOffset[2];
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
    float cameraAspectRatio;
    //! Specifies which value represents an invalid (un-initialized) value in the motion vectors buffer
    //! NOTE: This is only required if `cameraMotionIncluded` is set to false and SL needs to compute it.
    float motionVectorsInvalidValue;

	int viewHandle = 0;

	//! Specifies input texture size
	int inputSize[2];

	//! Specifies output texture size
	int outputSize[2];

    //! Specifies if depth values are inverted (value closer to the camera is higher) or not.
    bool depthInverted = true;
    //! Specifies if camera motion is included in the MVec buffer.
    bool cameraMotionIncluded = true;
    //! Specifies if motion vectors are 3D or not.
    bool motionVectors3D = false;
    //! Specifies if previous frame has no connection to the current one (i.e. motion vectors are invalid)
    bool reset = false;
    //! Specifies if orthographic projection is used or not.
    bool orthographicProjection = false;
    //! Specifies if motion vectors are already dilated or not.
    bool motionVectorsDilated = false;
    //! Specifies if motion vectors are jittered or not.
    bool motionVectorsJittered = true;

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
	eCount,
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
	UnityRenderBuffer m_DepthTexture;
	UnityRenderBuffer m_MotionVectorsTexture;
	UnityRenderBuffer m_HUDLessColorTexture;
	UnityRenderBuffer m_ScalingInputColorTexture;
	UnityRenderBuffer m_ScalingOutputColorTexture;
	CameraData m_CameraData;

public:
	virtual ~RenderAPI() { }

	// --------------------------------------------------------------------------
	// General plugin functions
	// --------------------------------------------------------------------------
	virtual void ProcessDeviceEvent(UnityGfxDeviceEventType type, IUnityInterfaces* interfaces) = 0;
	virtual bool ProcessRenderingExtQuery(UnityRenderingExtQueryType query) { return false; }
	virtual void SetCameraData(void* data) { m_CameraData = *static_cast<CameraData*>(data); }
	virtual void SetDepthTexture(UnityRenderBuffer renderBuffer) { m_DepthTexture = renderBuffer; }
	virtual void SetMotionVectorsTexture(UnityRenderBuffer renderBuffer) { m_MotionVectorsTexture = renderBuffer; }
	virtual void SetHUDLessColorTexture(UnityRenderBuffer renderBuffer) { m_HUDLessColorTexture = renderBuffer; }
	virtual void SetScalingInputColorTexture(UnityRenderBuffer renderBuffer) { m_ScalingInputColorTexture = renderBuffer; }
	virtual void SetScalingOutputColorTexture(UnityRenderBuffer renderBuffer) { m_ScalingOutputColorTexture = renderBuffer; }

    // --------------------------------------------------------------------------
    // Metal plugin specific functions
    // --------------------------------------------------------------------------
	virtual bool SupportMetalFX() { return false; }
    virtual void UpscaleTextureMetalFXSpatial() {}
	virtual void UpscaleTextureMetalFXTemporal() {}
    virtual void CleanupMetalFX() {}

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
	static IUnityProfiler* s_UnityProfiler;
	static const UnityProfilerMarkerDesc* s_ProfilerPresentMarker;
};

// Create a graphics API implementation instance for the given API type.
RenderAPI* CreateRenderAPI(UnityGfxRenderer apiType);
