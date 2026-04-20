#pragma once

// Metal implementation of RenderAPI.
#import <Metal/Metal.h>
#import <MetalFX/MetalFX.h>

#include "IUnityGraphicsMetal.h"
#include "RenderAPI.h"
#include "AccelerationStructure.h"

class API_AVAILABLE(ios(16.0), macos(13.0)) RenderAPI_Metal : public RenderAPI
{
public:
    RenderAPI_Metal();
    ~RenderAPI_Metal() override;

    virtual void ProcessDeviceEvent(UnityGfxDeviceEventType type, IUnityInterfaces* interfaces) override;

    virtual bool SupportMetalFX() override;
    virtual void UpscaleTextureMetalFXSpatial() override;
    virtual void UpscaleTextureMetalFXTemporal() override;
    virtual void CleanupMetalFX() override;
    virtual bool SupportRaytracing() override;
    virtual void DispatchRaytracing() override;
    virtual void CleanupRaytracing() override;
    virtual void SetBlasDescriptors(const BottomLevelAccelerationStructureDescriptor* blasDescriptors, const int* pSubmeshCount, int meshCount) override;
    virtual void SetTlasDescriptors(const TopLevelAccelerationStructureElementDescriptor* tlasDescriptor, int instanceCount) override;

private:
    void CreateResources();

private:
    IUnityGraphicsMetalV2*  m_MetalGraphics;
    API_AVAILABLE(ios(17.0), macos(14.0)) AccelerationStructure*  m_accelerationStructure;
    id<MTLFXSpatialScaler>  m_spatialScaler;
    id<MTLFXTemporalScaler> m_temporalScaler;
};
