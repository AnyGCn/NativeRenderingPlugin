#pragma once

#include "Unity/IUnityGraphics.h"
#include "Unity/IUnityLog.h"

#include <stddef.h>
#include <stdint.h>

struct IUnityInterfaces;

struct UpscaleTextureData
{
	int viewHandle;
	int frameID;
    void* inputColor;
    void* inputDepth;
    void* inputMotion;
    void* output;
	uint32_t inputSizeX;
	uint32_t inputSizeY;
	uint32_t outputSizeX;
	uint32_t outputSizeY;
    float jitterX;
    float jitterY;
    float motionVectorScaleX;
    float motionVectorScaleY;
};

// Super-simple "graphics abstraction". This is nothing like how a proper platform abstraction layer would look like;
// all this does is a base interface for whatever our plugin sample needs. Which is only "draw some triangles"
// and "modify a texture" at this point.
//
// There are implementations of this base class for D3D9, D3D11, OpenGL etc.; see individual RenderAPI_* files.
class RenderAPI
{
public:
	virtual ~RenderAPI() { }


	// Process general event like initialization, shutdown, device loss/reset etc.
	virtual void ProcessDeviceEvent(UnityGfxDeviceEventType type, IUnityInterfaces* interfaces) = 0;

    // --------------------------------------------------------------------------
    // Metal plugin specific functions
    // --------------------------------------------------------------------------


	virtual bool SupportMetalFX() { return false; }
    virtual void UpscaleTextureMetalFXSpatial(void* data) {}
	virtual void UpscaleTextureMetalFXTemporal(void* data) {}
    virtual void ClearResourceMetalFX(void* data) {}

	virtual bool SupportDLSS() { return false; }
	virtual void UpscaleTextureDLSS(void* data) {}

	virtual bool SupportFrameExtrapolate() { return false; }
	virtual void FrameExtrapolate(void* data) {}

	static IUnityLog* s_Logger;
};


// Create a graphics API implementation instance for the given API type.
RenderAPI* CreateRenderAPI(UnityGfxRenderer apiType);
