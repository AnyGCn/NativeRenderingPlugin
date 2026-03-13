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
#include "RenderAPI.h"
#include "SLWrapper.h"

#define ReturnOnFail(x, hr, OnFailureMsg, onFailureReturnValue) hr = x; if(FAILED(hr)){OutputDebugStringA(OnFailureMsg); return onFailureReturnValue;}

struct Vec3
{
    float x;
    float y;
    float z;
};

struct Vec4
{
    float x;
    float y;
    float z;
    float w;
};

struct Vertex
{
    Vec3 position;
    Vec4 color;
};

static void handle_hr(HRESULT hr, const char* error = "")
{
    if (FAILED(hr))
    {
        OutputDebugStringA(error);
        std::cerr << error << "\n";
        abort();
    }
}

struct D3D12MemoryObject
{
    ID3D12Resource* resource;
    void* mapped;
    D3D12_HEAP_TYPE heapType;
    D3D12_RESOURCE_FLAGS resourceFlags;
    UINT64 deviceMemorySize;
};

struct D3D12DefaultBufferMemoryObject
{
    D3D12MemoryObject uploadResource;
    D3D12MemoryObject defaultResource;
};

class RenderAPI_D3D12 : public RenderAPI
{
public:
    RenderAPI_D3D12();
    virtual ~RenderAPI_D3D12() override { }

    virtual void ProcessDeviceEvent(UnityGfxDeviceEventType type, IUnityInterfaces* interfaces) override;
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
    }
}

#undef ReturnOnFail

#endif // #if SUPPORT_D3D12
