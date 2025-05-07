
#include "RenderAPI.h"
#include "PlatformBase.h"


// Metal implementation of RenderAPI.


#if SUPPORT_METAL

#include "Unity/IUnityGraphicsMetal.h"
#import <Metal/Metal.h>
#import <MetalFX/MetalFX.h>

class RenderAPI_Metal : public RenderAPI
{
public:
	RenderAPI_Metal();
	virtual ~RenderAPI_Metal() { }

	virtual void ProcessDeviceEvent(UnityGfxDeviceEventType type, IUnityInterfaces* interfaces);

	virtual bool GetUsesReverseZ() { return true; }

    virtual void UpscaleTextureMetalFXSpatial(void* data);
    virtual void UpscaleTextureMetalFXTemporal(void* data);
    
private:
	void CreateResources();

private:
    IUnityGraphicsMetalV2*	m_MetalGraphics;
    id<MTLFXSpatialScaler>  m_spatialScaler;
    id<MTLFXTemporalScaler> m_temporalScaler;
};


RenderAPI* CreateRenderAPI_Metal()
{
	return new RenderAPI_Metal();
}

static Class MTLFXSpatialScalerDescriptorClass;
static Class MTLFXTemporalScalerDescriptorClass;

// Simple vertex & fragment shader source

void RenderAPI_Metal::CreateResources()
{
	id<MTLDevice> metalDevice = m_MetalGraphics->MetalDevice();
    m_spatialScaler = [description newSpatialScalerWithDevice:metalDevice];
    m_temporalScaler = [description newTemporalScalerWithDevice:metalDevice];
}

RenderAPI_Metal::RenderAPI_Metal()
{
}


void RenderAPI_Metal::ProcessDeviceEvent(UnityGfxDeviceEventType type, IUnityInterfaces* interfaces)
{
	if (type == kUnityGfxDeviceEventInitialize)
	{
		m_MetalGraphics = interfaces->Get<IUnityGraphicsMetalV2>();
        MTLFXSpatialScalerDescriptorClass = NSClassFromString(@"MTLFXSpatialScalerDescriptor");
        MTLFXTemporalScalerDescriptorClass = NSClassFromString(@"MTLFXTemporalScalerDescriptor");
		CreateResources();
	}
	else if (type == kUnityGfxDeviceEventShutdown)
	{
		//@TODO: release resources
	}
}

struct UpscaleTextureSpatialData
{
    void* input;
    void* output;
};

void RenderAPI_Metal::UpscaleTextureMetalFXSpatial(void* data) API_AVAILABLE(ios(16.0), macosx(13.0))
{
    UpscaleTextureSpatialData* upscaleTextureData = (UpscaleTextureSpatialData*)data;
    
    id<MTLTexture> input = (__bridge id<MTLTexture>)upscaleTextureData->input;
    id<MTLTexture> output = (__bridge id<MTLTexture>)upscaleTextureData->output;
    id<MTLDevice> metalDevice = m_MetalGraphics->MetalDevice();

    if (m_spatialScaler == nil ||
        m_spatialScaler.inputWidth != input.width ||
        m_spatialScaler.inputHeight != input.height ||
        m_spatialScaler.outputWidth != output.width ||
        m_spatialScaler.outputHeight != output.height ||
        m_spatialScaler.colorTextureFormat != input.pixelFormat ||
        m_spatialScaler.outputTextureFormat != output.pixelFormat)
    {
        MTLFXSpatialScalerDescriptor* description = [[MTLFXSpatialScalerDescriptorClass alloc] init];
        description.inputWidth = input.width;
        description.inputHeight = input.height;
        description.outputWidth = output.width;
        description.outputHeight = output.height;
        description.colorTextureFormat = input.pixelFormat;
        description.outputTextureFormat = output.pixelFormat;
        description.colorProcessingMode = MTLFXSpatialScalerColorProcessingModeLinear;
        
        m_spatialScaler = [description newSpatialScalerWithDevice:metalDevice];
    }
    
    id<MTLCommandBuffer> commandBuffer = m_MetalGraphics->CurrentCommandBuffer();
    if (commandBuffer == nil) return;
    
    m_MetalGraphics->EndCurrentCommandEncoder();
    m_spatialScaler.colorTexture = input;
    m_spatialScaler.outputTexture = output;
    
    [m_spatialScaler encodeToCommandBuffer:commandBuffer];
}

void RenderAPI_Metal::UpscaleTextureMetalFXTemporal(void* data) API_AVAILABLE(ios(16.0), macosx(13.0))
{
    
}

#endif // #if SUPPORT_METAL
