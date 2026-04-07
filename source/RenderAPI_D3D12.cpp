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

static void handle_hr(HRESULT hr, const char* error = "")
{
    if (FAILED(hr))
    {
        OutputDebugStringA(error);
        std::cerr << error << "\n";
        abort();
    }
}

class RenderAPI_D3D12 : public RenderAPI_D3D
{
public:
    RenderAPI_D3D12();
    ~RenderAPI_D3D12() override { }

    void ProcessDeviceEvent(UnityGfxDeviceEventType type, IUnityInterfaces* interfaces) override;
    bool ProcessRenderingExtQuery(UnityRenderingExtQueryType query) override;
    void UpscaleTextureDLSS() override;
    void CleanupDLSS() override;

    void* GetDevice() override { return m_Device; }
    virtual LUID GetAdapterLuid() override;
    sl::Resource AllocateBuffer(const sl::ResourceAllocationDesc* resDesc, void* device) override;
    sl::Resource AllocateTexture(const sl::ResourceAllocationDesc* resDesc, void* device) override;
    
private:
    IUnityGraphicsD3D12v7*  m_Graphics;
    ID3D12Device*           m_Device;

    ID3D12CommandAllocator*        m_streamline_cmd_allocator;
    ID3D12GraphicsCommandList*     m_streamline_cmd_list;
    UINT64                         m_streamline_fence = 0;

    HANDLE                  m_fence_event;

    void initialize_and_create_resources();
    void release_resources();
    void wait_for_unity_frame_fence(UINT64 fence_value);
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
        initialize_and_create_resources();
        SLWrapper::Get().Initialize(kUnityGfxRendererD3D12, this);
        {
            // Configure the Upscale_DLSS event (eventID = 2):
            // - FlushCommandBuffers: Unity submits all pending command buffers before invoking the
            //   plugin callback, so all tagged textures are in a known, finalized D3D12 state.
            // - ModifiesCommandBuffersState: SL inserts its own barriers/dispatches into the
            //   command list, so Unity must re-bind its pipeline state afterwards.
            UnityD3D12PluginEventConfig cfg{};
            cfg.graphicsQueueAccess = kUnityD3D12GraphicsQueueAccess_DontCare;
            cfg.flags = kUnityD3D12EventConfigFlag_EnsurePreviousFrameSubmission
                      | kUnityD3D12EventConfigFlag_SyncWorkerThreads
                      | kUnityD3D12EventConfigFlag_ModifiesCommandBuffersState;
            cfg.ensureActiveRenderTextureIsBound = false;
            m_Graphics->ConfigureEvent(Upscale_DLSS, &cfg);
            cfg.flags = kUnityD3D12EventConfigFlag_EnsurePreviousFrameSubmission
                      | kUnityD3D12EventConfigFlag_SyncWorkerThreads;
            m_Graphics->ConfigureEvent(Cleanup_DLSS, &cfg);
        }
        break;
    case kUnityGfxDeviceEventShutdown:
        SLWrapper::Get().Shutdown();
        release_resources();
        break;
    default:
        break;
    }
}

void RenderAPI_D3D12::initialize_and_create_resources()
{
    handle_hr(m_Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_streamline_cmd_allocator)),
              "Failed to create cmd allocator for streamline\n");

    handle_hr(m_Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_streamline_cmd_allocator, nullptr, IID_PPV_ARGS(&m_streamline_cmd_list)),
            "Failed to create streamline cmd list\n");

    m_streamline_cmd_allocator->SetName(L"streamline cmd allocator");
    m_streamline_cmd_list->SetName(L"streamline cmd list");

    handle_hr(m_streamline_cmd_list->Close(), "Failed to close streamline cmd list\n");
    m_fence_event = CreateEvent(NULL, false, false, nullptr);

}

void RenderAPI_D3D12::release_resources()
{
    SAFE_RELEASE(m_streamline_cmd_list);
    SAFE_RELEASE(m_streamline_cmd_allocator);

    CloseHandle(m_fence_event);
}

void RenderAPI_D3D12::wait_for_unity_frame_fence(UINT64 fence_value)
{
    ID3D12Fence* unity_fence = m_Graphics->GetFrameFence();
    UINT64 current_fence_value = unity_fence->GetCompletedValue();

    if (current_fence_value < fence_value)
    {
        handle_hr(unity_fence->SetEventOnCompletion(fence_value, m_fence_event), "Failed to set fence event on completion\n");
        WaitForSingleObject(m_fence_event, INFINITE);
    }
}

void RenderAPI_D3D12::UpscaleTextureDLSS()
{
    wait_for_unity_frame_fence(m_streamline_fence);
    
    m_streamline_cmd_allocator->Reset();
    m_streamline_cmd_list->Reset(m_streamline_cmd_allocator, nullptr);
    RenderAPI_D3D::UpscaleTextureDLSS(m_streamline_cmd_list);
    m_streamline_cmd_list->Close();

    constexpr uint32_t state_count = 4;
    UnityGraphicsD3D12ResourceState resource_states[state_count];
    resource_states[0] = { (ID3D12Resource*)m_Textures[eDepth], D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE };
    resource_states[1] = { (ID3D12Resource*)m_Textures[eMotionVectors], D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE };
    resource_states[2] = { (ID3D12Resource*)m_Textures[eScalingInputColor], D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE };
    resource_states[3] = { (ID3D12Resource*)m_Textures[eScalingOutputColor], D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_UNORDERED_ACCESS };
    m_streamline_fence = m_Graphics->ExecuteCommandList(m_streamline_cmd_list, state_count, resource_states);
}

void RenderAPI_D3D12::CleanupDLSS()
{
    wait_for_unity_frame_fence(m_streamline_fence);
    RenderAPI_D3D::CleanupDLSS();
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
    if (query & kUnityRenderingExtQueryOverridePresentFrame && m_Graphics->GetSwapChain() != nullptr)
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
