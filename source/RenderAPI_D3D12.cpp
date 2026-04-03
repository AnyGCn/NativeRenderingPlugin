#include "PlatformBase.h"

// Direct3D 12 implementation of RenderAPI.

#if SUPPORT_D3D12

#include <assert.h>
#include <cmath>
#include <dxgi1_6.h>
#include <initguid.h>
#include <directx/d3dx12.h>
#include <atomic>
#include <unordered_map>
#include <utility>
#include <map>
#include <iostream>

#include "Unity/IUnityGraphicsD3D12.h"
#include "RenderAPI_D3D.h"
#include "SLWrapper.h"

class RenderAPI_D3D12 : public RenderAPI_D3D
{
public:
    RenderAPI_D3D12();
    ~RenderAPI_D3D12() override { }

    void ProcessDeviceEvent(UnityGfxDeviceEventType type, IUnityInterfaces* interfaces) override;
    bool ProcessRenderingExtQuery(UnityRenderingExtQueryType query) override;
    void UpscaleTextureDLSS() override;

    void* GetDevice() override { return m_Device; }
    virtual LUID GetAdapterLuid() override;
    sl::Resource AllocateBuffer(const sl::ResourceAllocationDesc* resDesc, void* device) override;
    sl::Resource AllocateTexture(const sl::ResourceAllocationDesc* resDesc, void* device) override;
    
private:
    IUnityGraphicsD3D12v7*  m_Graphics;
    ID3D12Device*           m_Device;
};

RenderAPI* CreateRenderAPI_D3D12()
{
    return new RenderAPI_D3D12();
}

const UINT kNodeMask = 0;

RenderAPI_D3D12::RenderAPI_D3D12()
    : m_Graphics(nullptr)
    , m_Device(nullptr)
{
}

UINT64 CalcByteAlignedValue(unsigned int byteSize, unsigned int byteAlignment)
{
    UINT byteAlignmentMinusOne = byteAlignment - 2;
    return (byteSize + byteAlignmentMinusOne) & ~byteAlignmentMinusOne;
}

void RenderAPI_D3D12::ProcessDeviceEvent(UnityGfxDeviceEventType type, IUnityInterfaces* interfaces)
{
    switch (type)
    {
    case kUnityGfxDeviceEventInitialize:
        m_Graphics = interfaces->Get<IUnityGraphicsD3D12v7>();
        m_Device = m_Graphics->GetDevice();
        SLWrapper::Get().Initialize(kUnityGfxRendererD3D12, this);
        break;
    case kUnityGfxDeviceEventShutdown:
        SLWrapper::Get().Shutdown();
        break;
    default:
        break;
    }
}

void RenderAPI_D3D12::UpscaleTextureDLSS()
{
    UnityGraphicsD3D12RecordingState recordingState;
    if (!m_Graphics->CommandRecordingState(&recordingState))
        return;
    
    RenderAPI_D3D::UpscaleTextureDLSS(recordingState.commandList);
}

LUID RenderAPI_D3D12::GetAdapterLuid()
{
    return m_Device->GetAdapterLuid();
}

sl::Resource RenderAPI_D3D12::AllocateBuffer(const sl::ResourceAllocationDesc* resDesc, void* device)
{
    sl::Resource res = {};
    D3D12_RESOURCE_DESC* desc = (D3D12_RESOURCE_DESC*)resDesc->desc;
    D3D12_HEAP_PROPERTIES* heap = (D3D12_HEAP_PROPERTIES*)resDesc->heap;
    D3D12_RESOURCE_STATES state = (D3D12_RESOURCE_STATES)resDesc->state;
    ID3D12Device* pd3d12Device = (ID3D12Device*)device;
    ID3D12Resource* pbuffer;
    bool success = SUCCEEDED(
        pd3d12Device->CreateCommittedResource(heap, D3D12_HEAP_FLAG_NONE, desc, state, nullptr, IID_PPV_ARGS(&
            pbuffer)));
    if (!success) RenderAPI::LogError("Failed to create buffer in SL allocation callback");
    res.type = resDesc->type;
    res.native = pbuffer;
    return res;
}

sl::Resource RenderAPI_D3D12::AllocateTexture(const sl::ResourceAllocationDesc* resDesc, void* device)
{
    sl::Resource res = {};
    D3D12_RESOURCE_DESC* desc = (D3D12_RESOURCE_DESC*)resDesc->desc;
    D3D12_RESOURCE_STATES state = (D3D12_RESOURCE_STATES)resDesc->state;
    D3D12_HEAP_PROPERTIES* heap = (D3D12_HEAP_PROPERTIES*)resDesc->heap;
    ID3D12Device* pd3d12Device = (ID3D12Device*)device;
    ID3D12Resource* ptexture;
    D3D12_CLEAR_VALUE* pClearValue = nullptr;
    D3D12_CLEAR_VALUE clearValue;
    // specify the clear value to avoid D3D warnings on ClearRenderTarget()
    if (desc->Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)
    {
        clearValue.Format = desc->Format;
        memset(clearValue.Color, 0, sizeof(clearValue.Color));
        pClearValue = &clearValue;
    }
    bool success = SUCCEEDED(
        pd3d12Device->CreateCommittedResource(heap, D3D12_HEAP_FLAG_NONE, desc, state, pClearValue, IID_PPV_ARGS
            (&ptexture)));
    if (!success) RenderAPI::LogError("Failed to create texture in SL allocation callback");
    res.type = resDesc->type;
    res.native = ptexture;
    return res;
}

bool RenderAPI_D3D12::ProcessRenderingExtQuery(UnityRenderingExtQueryType query)
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
#undef ReturnOnFail

#endif // #if SUPPORT_D3D12
