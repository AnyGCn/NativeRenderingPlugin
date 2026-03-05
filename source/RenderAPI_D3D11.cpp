#include "PlatformBase.h"

// Direct3D 11 implementation of RenderAPI.

#if SUPPORT_D3D11

#include <assert.h>
#include <d3d11.h>

#include "RenderAPI.h"
#include "SLWrapper.h"
#include "Unity/IUnityGraphicsD3D11.h"


class RenderAPI_D3D11 : public RenderAPI
{
public:
    RenderAPI_D3D11();

    virtual ~RenderAPI_D3D11()
    {
    }

    virtual void ProcessDeviceEvent(UnityGfxDeviceEventType type, IUnityInterfaces* interfaces);
	virtual bool SupportDLSS();
	virtual void UpscaleTextureDLSS(void* data);

private:
    ID3D11Device* m_Device;
};

RenderAPI* CreateRenderAPI_D3D11()
{
    return new RenderAPI_D3D11();
}

RenderAPI_D3D11::RenderAPI_D3D11()
    : m_Device(NULL)
{
}

void RenderAPI_D3D11::ProcessDeviceEvent(UnityGfxDeviceEventType type, IUnityInterfaces* interfaces)
{
    switch (type)
    {
    case kUnityGfxDeviceEventInitializePre:
        SLWrapper::Get().Initialize_preDevice(kUnityGfxRendererD3D11);
        break;
    case kUnityGfxDeviceEventInitialize:
        {
            IUnityGraphicsD3D11* d3d = interfaces->Get<IUnityGraphicsD3D11>();
            m_Device = d3d->GetDevice();
            SLWrapper::Get().Initialize(kUnityGfxRendererD3D11, m_Device);
            break;
        }
    case kUnityGfxDeviceEventShutdown:
        SLWrapper::Get().Shutdown();
        break;
    default:
        break;
    }
}

bool RenderAPI_D3D11::SupportDLSS()
{
    return SLWrapper::Get().GetDLSSAvailable();
}

void RenderAPI_D3D11::UpscaleTextureDLSS(void* data)
{
    UpscaleTextureData* upscaleData = (UpscaleTextureData*)data;
    SLWrapper::RenderSurfaceSettings renderSurfaceSettings = { upscaleData->inputSizeX, upscaleData->inputSizeY, upscaleData->outputSizeX, upscaleData->outputSizeY };
    const bool isReset = SLWrapper::Get().CleanupDLSSIfNeeded(renderSurfaceSettings);
    SLWrapper::Get().SetViewportHandle(sl::ViewportHandle{ upscaleData->viewHandle }, upscaleData->frameID);
    sl::Constants slConstants;
    slConstants.cameraPos = { 0.0f, 0.0f, 0.0f };
    slConstants.cameraUp = { 0.0f, 1.0f, 0.0f };
    slConstants.cameraFwd = { 0.0f, 0.0f, 1.0f };
    slConstants.cameraRight = { 1.0f, 0.0f, 0.0f };
    slConstants.cameraNear = 0.3f;
    slConstants.cameraFar = 1000.0f;
    slConstants.cameraFOV = 60.0f * 3.14159265358979323846 / 180.0f;
    slConstants.cameraAspectRatio = (float)upscaleData->outputSizeX / (float)upscaleData->outputSizeY;
    slConstants.jitterOffset = { upscaleData->jitterX, upscaleData->jitterY };
    slConstants.mvecScale = { upscaleData->motionVectorScaleX, upscaleData->motionVectorScaleY };
    slConstants.depthInverted = sl::eTrue;
    slConstants.cameraMotionIncluded = sl::eTrue;
    slConstants.motionVectors3D = sl::eFalse;
    slConstants.reset = isReset ? sl::eTrue : sl::eFalse;

    SLWrapper::Get().SetSLConsts(renderSurfaceSettings, slConstants);
    sl::DLSSOptions dlssOptions;
    SLWrapper::DLSSSettings dlssSettings;
    dlssOptions.outputWidth = upscaleData->outputSizeX;
    dlssOptions.outputHeight = upscaleData->outputSizeY;
    if (isReset)
    {
        if (renderSurfaceSettings.renderSizeX == renderSurfaceSettings.outputSizeX && renderSurfaceSettings.renderSizeY == renderSurfaceSettings.outputSizeY)
        {
            dlssOptions.mode = sl::DLSSMode::eDLAA;
            SLWrapper::Get().SetDLSSOptions(dlssOptions);
            SLWrapper::Get().QueryDLSSOptimalSettings(dlssSettings);
        }
        else
        {
            for (sl::DLSSMode mode = sl::DLSSMode::eMaxPerformance; mode <= sl::DLSSMode::eUltraQuality; mode = (sl::DLSSMode)((int)mode + 1))
            {
                dlssOptions.mode = mode;
                SLWrapper::Get().SetDLSSOptions(dlssOptions);
                SLWrapper::Get().QueryDLSSOptimalSettings(dlssSettings);
                if (dlssSettings.maxRenderSizeX >= renderSurfaceSettings.renderSizeX && dlssSettings.maxRenderSizeY >= renderSurfaceSettings.renderSizeY)
                {
                    break;
                }
            }
        }
    }

    ID3D11DeviceContext* context = nullptr;
    m_Device->GetImmediateContext(&context);
    SLWrapper::Get().TagResources_General(context, upscaleData->inputMotion, upscaleData->inputDepth, upscaleData->output);
    SLWrapper::Get().TagResources_DLSS_NIS(context, upscaleData->output, upscaleData->inputColor);
    SLWrapper::Get().EvaluateDLSS(context);
    context->Release();
}

#endif // #if SUPPORT_D3D11
