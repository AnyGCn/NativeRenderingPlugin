// Example low level rendering Unity plugin

#include "PlatformBase.h"

#include <cassert>
#include <vector>

#include "RenderAPI.h"

// --------------------------------------------------------------------------
// UnitySetInterfaces

static void UNITY_INTERFACE_API OnGraphicsDeviceEvent(UnityGfxDeviceEventType eventType);

static IUnityInterfaces *s_UnityInterfaces = NULL;
static IUnityGraphics *s_Graphics = NULL;
IUnityLog *RenderAPI::s_Logger = NULL;
IUnityProfiler *RenderAPI::s_UnityProfiler = NULL;
const UnityProfilerMarkerDesc *RenderAPI::s_ProfilerPresentMarker = NULL;

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API UnityPluginLoad(IUnityInterfaces *unityInterfaces)
{
	s_UnityInterfaces = unityInterfaces;
	s_Graphics = s_UnityInterfaces->Get<IUnityGraphics>();
	RenderAPI::s_Logger = s_UnityInterfaces->Get<IUnityLog>();
	RenderAPI::s_UnityProfiler = unityInterfaces->Get<IUnityProfiler>();
	s_Graphics->RegisterDeviceEventCallback(OnGraphicsDeviceEvent);
	RenderAPI::s_UnityProfiler->CreateMarker(&RenderAPI::s_ProfilerPresentMarker, "Swapchain::PresentCustomByGfxPlugin", kUnityProfilerCategoryRender, kUnityProfilerMarkerFlagDefault, 0);

	if (s_Graphics->GetRenderer() == kUnityGfxRendererNull)
	{
#if SUPPORT_VULKAN
		extern void RenderAPI_Vulkan_OnPluginLoad();
		RenderAPI_Vulkan_OnPluginLoad();
#endif // SUPPORT_VULKAN

#if SUPPORT_D3D11 || SUPPORT_D3D12
		extern void OnPluginLoad_D3D();
		OnPluginLoad_D3D();
#endif // if SUPPORT_D3D11
	}

	// Run OnGraphicsDeviceEvent(initialize) manually on plugin load
	OnGraphicsDeviceEvent(kUnityGfxDeviceEventInitialize);
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API UnityPluginUnload()
{
	s_Graphics->UnregisterDeviceEventCallback(OnGraphicsDeviceEvent);
}

#if UNITY_WEBGL
typedef void(UNITY_INTERFACE_API *PluginLoadFunc)(IUnityInterfaces *unityInterfaces);
typedef void(UNITY_INTERFACE_API *PluginUnloadFunc)();

extern "C" void UnityRegisterRenderingPlugin(PluginLoadFunc loadPlugin, PluginUnloadFunc unloadPlugin);

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API RegisterPlugin()
{
	UnityRegisterRenderingPlugin(UnityPluginLoad, UnityPluginUnload);
}
#endif

// --------------------------------------------------------------------------
// GraphicsDeviceEvent

static RenderAPI *s_CurrentAPI = NULL;
static UnityGfxRenderer s_DeviceType = kUnityGfxRendererNull;

static void UNITY_INTERFACE_API OnGraphicsDeviceEvent(UnityGfxDeviceEventType eventType)
{
	// Create graphics API implementation upon initialization
	if (eventType == kUnityGfxDeviceEventInitialize)
	{
		assert(s_CurrentAPI == NULL);
		s_DeviceType = s_Graphics->GetRenderer();
		s_CurrentAPI = CreateRenderAPI(s_DeviceType);
	}

	// Let the implementation process the device related events
	if (s_CurrentAPI)
	{
		s_CurrentAPI->ProcessDeviceEvent(eventType, s_UnityInterfaces);
	}

	// Cleanup graphics API implementation upon shutdown
	if (eventType == kUnityGfxDeviceEventShutdown)
	{
		delete s_CurrentAPI;
		s_CurrentAPI = NULL;
		s_DeviceType = kUnityGfxRendererNull;
	}
}

extern "C" bool UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API UnityRenderingExtQuery(UnityRenderingExtQueryType query)
{
	return s_CurrentAPI ? s_CurrentAPI->ProcessRenderingExtQuery(query) : false;
}

// --------------------------------------------------------------------------
// OnRenderEvent
// This will be called for GL.IssuePluginEvent script calls; eventID will
// be the integer passed to IssuePluginEvent. In this example, we just ignore
// that value.
enum RenderEventType
{
	Sync_RenderStart,
	Sync_RenderEnd,
	Upscale_DLSS,
	Cleanup_DLSS,
	Upscale_MetalFX_Spatial,
	Upscale_MetalFX_Temporal,
	Cleanup_MetalFX,
	RenderEventCount,
};

static void UNITY_INTERFACE_API OnRenderEventWithData(int eventID, void *data)
{
	// Unknown / unsupported graphics device type? Do nothing
	if (s_CurrentAPI == NULL || eventID < 0 || eventID >= RenderEventCount)
	{
		return;
	}

	switch (eventID)
	{
	case Sync_RenderStart:
		s_CurrentAPI->ReflexCallback_RenderStart(reinterpret_cast<uint32_t>(data));
		break;
	case Sync_RenderEnd:
		s_CurrentAPI->ReflexCallback_RenderEnd(reinterpret_cast<uint32_t>(data));
		break;
	case Upscale_DLSS:
		s_CurrentAPI->UpscaleTextureDLSS();
		break;
	case Cleanup_DLSS:
		s_CurrentAPI->CleanupDLSS();
		break;
	case Upscale_MetalFX_Spatial:
		s_CurrentAPI->UpscaleTextureMetalFXSpatial();
		break;
	case Upscale_MetalFX_Temporal:
		s_CurrentAPI->UpscaleTextureMetalFXTemporal();
		break;
	case Cleanup_MetalFX:
		s_CurrentAPI->CleanupMetalFX();
		break;
	default:
		break;
	}
}

// --------------------------------------------------------------------------
// extern render event function pointer
extern "C" UnityRenderingEventAndData UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API GetRenderEventWithDataFunc()
{
	return OnRenderEventWithData;
}

// --------------------------------------------------------------------------
// extern function
extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API SetCameraData(void* data)
{
	s_CurrentAPI->SetCameraData(data);
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API SetDepthTexture(UnityRenderBuffer renderBuffer)
{
	s_CurrentAPI->SetDepthTexture(renderBuffer);
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API SetMotionVectorsTexture(UnityRenderBuffer renderBuffer)
{
	s_CurrentAPI->SetMotionVectorsTexture(renderBuffer);
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API SetHUDLessColorTexture(UnityRenderBuffer renderBuffer)
{
	s_CurrentAPI->SetHUDLessColorTexture(renderBuffer);
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API SetScalingInputColorTexture(UnityRenderBuffer renderBuffer)
{
	s_CurrentAPI->SetScalingInputColorTexture(renderBuffer);
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API SetScalingOutputColorTexture(UnityRenderBuffer renderBuffer)
{
	s_CurrentAPI->SetScalingOutputColorTexture(renderBuffer);
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API Sync_Sleep(int frameID)
{
	s_CurrentAPI->ReflexCallback_Sleep(frameID);
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API Sync_SimulateBegin(int frameID)
{
	s_CurrentAPI->ReflexCallback_SimStart(frameID);
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API Sync_SimulateEnd(int frameID)
{
	s_CurrentAPI->ReflexCallback_SimEnd(frameID);
}

extern "C" bool UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API SupportMetalFX()
{
	return s_CurrentAPI->SupportMetalFX();
}

extern "C" bool UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API SupportDLSS()
{
	return s_CurrentAPI->SupportDLSS();
}

extern "C" bool UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API SupportDLSS_FG()
{
	return s_CurrentAPI->SupportFrameExtrapolate();
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API SetDLSSOptions(DLSSMode mode)
{
	s_CurrentAPI->SetDLSSOptions(mode);
}

extern "C" DLSSSettings UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API QueryDLSSOptimalSettings(DLSSMode mode)
{
	return s_CurrentAPI->QueryDLSSOptimalSettings(mode);
}
