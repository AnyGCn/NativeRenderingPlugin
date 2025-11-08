
#include "RenderAPI.h"
#include "PlatformBase.h"


// Metal implementation of RenderAPI.


#if SUPPORT_METAL

#include "Unity/IUnityGraphicsMetal.h"
#import <Metal/Metal.h>
#import <MetalFX/MetalFX.h>

class API_AVAILABLE(macos(13.0)) API_AVAILABLE(macos(13.0)) RenderAPI_Metal : public RenderAPI
{
public:
	RenderAPI_Metal();
	virtual ~RenderAPI_Metal() { }

	virtual void ProcessDeviceEvent(UnityGfxDeviceEventType type, IUnityInterfaces* interfaces);

	virtual bool GetUsesReverseZ() { return true; }
    
    virtual void UpscaleTextureMetalFXSpatial(void* data);
    virtual void UpscaleTextureMetalFXTemporal(void* data);
    virtual void ClearResourceMetalFX(void* data);
private:
	void CreateResources();

private:
    IUnityGraphicsMetalV2*	m_MetalGraphics;
    id<MTLFXSpatialScaler>  m_spatialScaler;
    id<MTLFXTemporalScaler> m_temporalScaler;
};


RenderAPI* CreateRenderAPI_Metal()
{
    if (@available(iOS 16.0, macOS 13.0, *))
        return new RenderAPI_Metal();
    
    return nullptr;
}

static Class MTLFXSpatialScalerDescriptorClass;
static Class MTLFXTemporalScalerDescriptorClass;

// Simple vertex & fragment shader source

void RenderAPI_Metal::CreateResources()
{
    
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
    m_temporalScaler = nil;

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

struct UpscaleTextureTemporalData
{
    void* inputColor;
    void* inputDepth;
    void* inputMotion;
    void* output;
    float jitterX;
    float jitterY;
    int reset;
};

void RenderAPI_Metal::UpscaleTextureMetalFXTemporal(void* data) API_AVAILABLE(ios(16.0), macosx(13.0))
{
    UpscaleTextureTemporalData* upscaleTextureData = (UpscaleTextureTemporalData*)data;
    
    id<MTLTexture> input = (__bridge id<MTLTexture>)upscaleTextureData->inputColor;
    id<MTLTexture> depth = (__bridge id<MTLTexture>)upscaleTextureData->inputDepth;
    id<MTLTexture> motion = (__bridge id<MTLTexture>)upscaleTextureData->inputMotion;
    id<MTLTexture> output = (__bridge id<MTLTexture>)upscaleTextureData->output;
    id<MTLDevice> metalDevice = m_MetalGraphics->MetalDevice();
    m_spatialScaler = nil;

    if (m_temporalScaler == nil ||
        m_temporalScaler.inputWidth != input.width ||
        m_temporalScaler.inputHeight != input.height ||
        m_temporalScaler.outputWidth != output.width ||
        m_temporalScaler.outputHeight != output.height ||
        m_temporalScaler.colorTextureFormat != input.pixelFormat ||
        m_temporalScaler.outputTextureFormat != output.pixelFormat ||
        m_temporalScaler.depthTextureFormat != depth.pixelFormat ||
        m_temporalScaler.motionTextureFormat != motion.pixelFormat)
    {
        MTLFXTemporalScalerDescriptor* description = [[MTLFXTemporalScalerDescriptorClass alloc] init];
        description.inputWidth = input.width;
        description.inputHeight = input.height;
        description.outputWidth = output.width;
        description.outputHeight = output.height;
        description.colorTextureFormat = input.pixelFormat;
        description.outputTextureFormat = output.pixelFormat;
        description.depthTextureFormat = depth.pixelFormat;
        description.motionTextureFormat = motion.pixelFormat;
        
        m_temporalScaler = [description newTemporalScalerWithDevice:metalDevice];
        m_temporalScaler.motionVectorScaleX = input.width;
        m_temporalScaler.motionVectorScaleY = input.height;
        m_temporalScaler.reset = true;
    }
    else
    {
        m_temporalScaler.reset = false;
    }
    
    id<MTLCommandBuffer> commandBuffer = m_MetalGraphics->CurrentCommandBuffer();
    if (commandBuffer == nil) return;
    
    m_MetalGraphics->EndCurrentCommandEncoder();
    m_temporalScaler.colorTexture = input;
    m_temporalScaler.depthTexture = depth;
    m_temporalScaler.motionTexture = motion;
    m_temporalScaler.outputTexture = output;
    m_temporalScaler.reset |= upscaleTextureData->reset == 1;
    m_temporalScaler.jitterOffsetX = upscaleTextureData->jitterX;
    m_temporalScaler.jitterOffsetY = upscaleTextureData->jitterY;
    
    [m_temporalScaler encodeToCommandBuffer:commandBuffer];
}

void RenderAPI_Metal::ClearResourceMetalFX(void* data)
{
    m_spatialScaler = nil;
    m_temporalScaler = nil;
}

bool SupportMetalFX()
{
    if (@available(ios 16.0, macOS 13.0, *))
        return true;
    else
        return false;
}

#endif // #if SUPPORT_METAL
