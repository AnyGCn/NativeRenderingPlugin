#include "PlatformBase.h"

#if SUPPORT_METAL

#include "ShaderDefinition.h"
#include "RenderAPI_Metal.h"

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

RenderAPI_Metal::~RenderAPI_Metal()
{

}

void RenderAPI_Metal::ProcessDeviceEvent(UnityGfxDeviceEventType type, IUnityInterfaces* interfaces)
{
    if (type == kUnityGfxDeviceEventInitialize)
    {
        m_MetalGraphics = interfaces->Get<IUnityGraphicsMetalV2>();
        MTLFXSpatialScalerDescriptorClass = NSClassFromString(@"MTLFXSpatialScalerDescriptor");
        MTLFXTemporalScalerDescriptorClass = NSClassFromString(@"MTLFXTemporalScalerDescriptor");
        if (@available(iOS 17.0, macOS 14.0, *))
        {
            m_accelerationStructure = new AccelerationStructure();
            m_accelerationStructure->Initialize(m_MetalGraphics->MetalDevice());
        }

        CreateResources();
    }
    else if (type == kUnityGfxDeviceEventShutdown)
    {
        //@TODO: release resources
        if (@available(iOS 17.0, macOS 14.0, *))
        {
            m_accelerationStructure->CleanupRaytracing();
            delete m_accelerationStructure;
        }
    }
}

void RenderAPI_Metal::UpscaleTextureMetalFXSpatial() API_AVAILABLE(ios(16.0), macosx(13.0))
{
    id<MTLTexture> input = (__bridge id<MTLTexture>)m_Textures[eScalingInputColor];
    id<MTLTexture> output = (__bridge id<MTLTexture>)m_Textures[eScalingOutputColor];
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
        description.colorProcessingMode = m_CameraData.colorBuffersHDR
            ? MTLFXSpatialScalerColorProcessingModeHDR
            : MTLFXSpatialScalerColorProcessingModeLinear;
        
        m_spatialScaler = [description newSpatialScalerWithDevice:metalDevice];
    }
    
    id<MTLCommandBuffer> commandBuffer = m_MetalGraphics->CurrentCommandBuffer();
    if (commandBuffer == nil) return;
    
    m_MetalGraphics->EndCurrentCommandEncoder();
    m_spatialScaler.colorTexture = input;
    m_spatialScaler.outputTexture = output;
    
    [m_spatialScaler encodeToCommandBuffer:commandBuffer];
}

void RenderAPI_Metal::UpscaleTextureMetalFXTemporal() API_AVAILABLE(ios(16.0), macosx(13.0))
{
    id<MTLTexture> input = (__bridge id<MTLTexture>)m_Textures[eScalingInputColor];
    id<MTLTexture> depth = (__bridge id<MTLTexture>)m_Textures[eDepth];
    id<MTLTexture> motion = (__bridge id<MTLTexture>)m_Textures[eMotionVectors];
    id<MTLTexture> output = (__bridge id<MTLTexture>)m_Textures[eScalingOutputColor];
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
    m_temporalScaler.reset |= m_CameraData.reset;
    m_temporalScaler.jitterOffsetX = m_CameraData.jitterOffset.x;
    m_temporalScaler.jitterOffsetY = m_CameraData.jitterOffset.y;
    m_temporalScaler.motionVectorScaleX = m_CameraData.mvecScale.x * m_temporalScaler.inputWidth;
    m_temporalScaler.motionVectorScaleY = m_CameraData.mvecScale.y * m_temporalScaler.inputHeight;
    
    [m_temporalScaler encodeToCommandBuffer:commandBuffer];
}

void RenderAPI_Metal::CleanupMetalFX()
{
    m_spatialScaler = nil;
    m_temporalScaler = nil;
}

bool RenderAPI_Metal::SupportMetalFX()
{
    if (@available(ios 16.0, macOS 13.0, *))
        return true;
    else
        return false;
}

bool RenderAPI_Metal::SupportRaytracing()
{
    if (@available(iOS 17.0, macOS 14.0, *))
        return m_accelerationStructure->IsSupported();
    else
        return false;
}

void RenderAPI_Metal::SetRaytracingInstances(const InstanceDescriptor* pInstances, int count) API_AVAILABLE(ios(17.0), macosx(14.0))
{
    m_accelerationStructure->SetInstances(pInstances, count);
}

void RenderAPI_Metal::SetRaytracingMaterials(const MaterialDscriptor* pMaterials, int count) API_AVAILABLE(ios(17.0), macosx(14.0))
{
    m_accelerationStructure->SetMaterials(pMaterials, count);
}

void RenderAPI_Metal::SetRaytracingMeshes(const MeshDescriptor* pMeshes, int count) API_AVAILABLE(ios(17.0), macosx(14.0))
{
    m_accelerationStructure->SetMeshes(pMeshes, count);
}

void RenderAPI_Metal::SetRaytracingGeometryBuildRequestList(const int* pBuildRequestList, int count) API_AVAILABLE(ios(17.0), macosx(14.0))
{
    m_accelerationStructure->SetRaytracingGeometryBuildRequestList(pBuildRequestList, count);
}

void RenderAPI_Metal::SetRaytracingRenderParameters(const RaytracingRenderParameters& lightData) API_AVAILABLE(ios(17.0), macosx(14.0))
{
    m_accelerationStructure->SetRenderParameters(lightData);
}

void RenderAPI_Metal::DispatchRaytracing() API_AVAILABLE(ios(17.0), macosx(14.0))
{
    id<MTLCommandBuffer> commandBuffer = m_MetalGraphics->CurrentCommandBuffer();
    if (commandBuffer == nil) return;
    __unsafe_unretained id<MTLTexture> textures[AAPLRaytracingTextureCount];
    textures[AAPLRaytracingOutImageIndex] = (__bridge id<MTLTexture>)m_Textures[eRaytracingOutput];
    textures[AAPLRaytracingGBufferDepthIndex] = (__bridge id<MTLTexture>)m_Textures[eDepth];
    textures[AAPLRaytracingGBufferNormalIndex] = (__bridge id<MTLTexture>)m_Textures[eNormal];
    textures[AAPLRaytracingGBufferMaskIndex] = (__bridge id<MTLTexture>)m_Textures[eGBufferMask];
    textures[AAPLRaytracingMainLightShadowMap] = (__bridge id<MTLTexture>)m_Textures[eMainLightShadowMap];
    textures[AAPLRaytracingSkyCubeMap] = (__bridge id<MTLTexture>)m_Textures[eSkyCube];
    m_MetalGraphics->EndCurrentCommandEncoder();
    m_accelerationStructure->DispatchRaytracing(commandBuffer, textures);
}

void RenderAPI_Metal::CleanupRaytracing() API_AVAILABLE(ios(17.0), macosx(14.0))
{
    m_accelerationStructure->CleanupRaytracing();
}

#endif // #if SUPPORT_METAL
