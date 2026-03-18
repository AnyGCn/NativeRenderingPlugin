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
        SLWrapper::Get().Initialize(kUnityGfxRendererD3D12, m_Device);
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
