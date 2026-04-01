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

    virtual void* GetDevice() override { return m_Device; }
    virtual sl::AdapterInfo GetAdaptInfo() override;
    sl::Resource AllocateBuffer(const sl::ResourceAllocationDesc* resDesc, void* device) override;
    sl::Resource AllocateTexture(const sl::ResourceAllocationDesc* resDesc, void* device) override;
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
            SLWrapper::Get().Initialize(kUnityGfxRendererD3D11, this);
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

sl::AdapterInfo RenderAPI_D3D11::GetAdaptInfo()
{
    sl::AdapterInfo adapterInfo;
    IDXGIDevice* pDxgiDevice;
    if (SUCCEEDED((m_Device)->QueryInterface(&pDxgiDevice)))
    {
        IDXGIAdapter* pAdapter;
        if (SUCCEEDED(pDxgiDevice->GetAdapter(&pAdapter)))
        {
            DXGI_ADAPTER_DESC desc;
            pAdapter->GetDesc(&desc);
            adapterInfo.deviceLUID = (uint8_t*)&desc.AdapterLuid;
            adapterInfo.deviceLUIDSizeInBytes = sizeof(LUID);
        }
    }

    return adapterInfo;
}

sl::Resource RenderAPI_D3D11::AllocateBuffer(const sl::ResourceAllocationDesc* resDesc, void* device)
{
    sl::Resource res = {};
    D3D11_BUFFER_DESC* desc = (D3D11_BUFFER_DESC*)resDesc->desc;
    ID3D11Device* pd3d11Device = (ID3D11Device*)device;
    ID3D11Buffer* pbuffer;
    bool success = SUCCEEDED(m_Device->CreateBuffer(desc, nullptr, &pbuffer));
    if (!success) RenderAPI::LogError("Failed to create buffer in SL allocation callback");
    res.type = resDesc->type;
    res.native = pbuffer;
    return res;
}

sl::Resource RenderAPI_D3D11::AllocateTexture(const sl::ResourceAllocationDesc* resDesc, void* device)
{
    sl::Resource res = {};
    D3D11_TEXTURE2D_DESC* desc = (D3D11_TEXTURE2D_DESC*)resDesc->desc;
    ID3D11Device* pd3d11Device = (ID3D11Device*)device;
    ID3D11Texture2D* ptexture;
    bool success = SUCCEEDED(pd3d11Device->CreateTexture2D(desc, nullptr, &ptexture));
    if (!success) RenderAPI::LogError("Failed to create texture in SL allocation callback");
    res.type = resDesc->type;
    res.native = ptexture;
    return res;
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
