#include "PlatformBase.h"

// Direct3D 11 implementation of RenderAPI.

#if SUPPORT_D3D11

#include <assert.h>
#include <d3d11.h>

#include "RenderAPI_D3D.h"
#include "SLWrapper.h"
#include "Unity/IUnityGraphicsD3D11.h"

class RenderAPI_D3D11 : public RenderAPI_D3D
{
public:
    RenderAPI_D3D11();
    ~RenderAPI_D3D11() override { }

    void ProcessDeviceEvent(UnityGfxDeviceEventType type, IUnityInterfaces* interfaces) override;
    bool ProcessRenderingExtQuery(UnityRenderingExtQueryType query) override;
	void UpscaleTextureDLSS() override;
private:
    IUnityGraphicsD3D11*    m_Graphics;
    ID3D11Device*           m_Device;
};

RenderAPI* CreateRenderAPI_D3D11()
{
    return new RenderAPI_D3D11();
}

RenderAPI_D3D11::RenderAPI_D3D11()
    : m_Graphics(nullptr)
    , m_Device(nullptr)
{
}

void RenderAPI_D3D11::ProcessDeviceEvent(UnityGfxDeviceEventType type, IUnityInterfaces* interfaces)
{
    UNITY_LOG(RenderAPI::s_Logger, "RenderAPI_D3D11::ProcessDeviceEvent");
    switch (type)
    {
    case kUnityGfxDeviceEventInitialize:
        {
            m_Graphics = interfaces->Get<IUnityGraphicsD3D11>();
            m_Device = m_Graphics->GetDevice();
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

void RenderAPI_D3D11::UpscaleTextureDLSS()
{
    ID3D11DeviceContext* context = nullptr;
    m_Device->GetImmediateContext(&context);
    RenderAPI_D3D::UpscaleTextureDLSS(context);
    context->Release();
}

bool RenderAPI_D3D11::ProcessRenderingExtQuery(UnityRenderingExtQueryType query)
{
    if (query & kUnityRenderingExtQueryOverridePresentFrame)
    {
        RenderAPI::s_UnityProfiler->BeginSample(RenderAPI::s_ProfilerPresentMarker);
        SLWrapper::Get().ReflexCallback_PresentStart();
        m_Graphics->GetSwapChain()->Present(m_Graphics->GetSyncInterval(), m_Graphics->GetPresentFlags());
        SLWrapper::Get().ReflexCallback_PresentEnd();
        RenderAPI::s_UnityProfiler->EndSample(RenderAPI::s_ProfilerPresentMarker);
        return true;
    }

    return false;
}
#endif // #if SUPPORT_D3D11
