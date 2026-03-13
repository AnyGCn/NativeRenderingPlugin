// Example low level rendering Unity plugin

#include "PlatformBase.h"

#include <assert.h>
#include <math.h>
#include <vector>

#include "RenderAPI.h"

// --------------------------------------------------------------------------
// UnitySetInterfaces

static void UNITY_INTERFACE_API OnGraphicsDeviceEvent(UnityGfxDeviceEventType eventType);

static IUnityInterfaces *s_UnityInterfaces = NULL;
static IUnityGraphics *s_Graphics = NULL;
IUnityLog *RenderAPI::s_Logger = NULL;

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API UnityPluginLoad(IUnityInterfaces *unityInterfaces)
{
	s_UnityInterfaces = unityInterfaces;
	s_Graphics = s_UnityInterfaces->Get<IUnityGraphics>();
	RenderAPI::s_Logger = s_UnityInterfaces->Get<IUnityLog>();
	RenderAPI::s_UnityProfiler = unityInterfaces->Get<IUnityProfiler>();
	s_Graphics->RegisterDeviceEventCallback(OnGraphicsDeviceEvent);
	RenderAPI::s_UnityProfiler->CreateMarker(&RenderAPI::s_ProfilerPresentMarker, "Swapchain::PresentCustomByGfxPlugin", kUnityProfilerCategoryRender, kUnityProfilerMarkerFlagDefault, 0);

	if (s_Graphics->GetRenderer() == kUnityGfxRendererNull)
		RenderAPI_OnPluginLoad();

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

const int RenderEventWithDataFuncCount = 5;
static RenderAPI *s_CurrentAPI = NULL;
static UnityGfxRenderer s_DeviceType = kUnityGfxRendererNull;
typedef void (RenderAPI::*RenderEventWithDataFunc)(void *);
RenderEventWithDataFunc s_RenderEventWithDataFunc[RenderEventWithDataFuncCount];

static void UNITY_INTERFACE_API OnGraphicsDeviceEvent(UnityGfxDeviceEventType eventType)
{
	// Create graphics API implementation upon initialization
	if (eventType == kUnityGfxDeviceEventInitialize)
	{
		assert(s_CurrentAPI == NULL);
		s_DeviceType = s_Graphics->GetRenderer();
		s_CurrentAPI = CreateRenderAPI(s_DeviceType);
		s_RenderEventWithDataFunc[0] = &RenderAPI::UpscaleTextureMetalFXSpatial;
		s_RenderEventWithDataFunc[1] = &RenderAPI::UpscaleTextureMetalFXTemporal;
		s_RenderEventWithDataFunc[2] = &RenderAPI::ClearResourceMetalFX;
		s_RenderEventWithDataFunc[3] = &RenderAPI::FrameExtrapolate;
		s_RenderEventWithDataFunc[4] = &RenderAPI::UpscaleTextureDLSS;
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

static void UNITY_INTERFACE_API OnRenderEvent(int eventID)
{
	// Unknown / unsupported graphics device type? Do nothing
	if (s_CurrentAPI == NULL)
		return;
}

static void UNITY_INTERFACE_API OnRenderEventWithData(int eventID, void *data)
{
	// Unknown / unsupported graphics device type? Do nothing
	if (s_CurrentAPI == NULL || eventID < 0 || eventID >= RenderEventWithDataFuncCount)
	{
		return;
	}

	(s_CurrentAPI->*(s_RenderEventWithDataFunc[eventID]))(data);
}

// --------------------------------------------------------------------------
// extern render event function pointer
extern "C" UnityRenderingEvent UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API GetRenderEvent_RenderStart()
{
	return [](int frameID) { if (s_CurrentAPI != NULL) s_CurrentAPI->ReflexCallback_RenderStart(frameID); };
}

extern "C" UnityRenderingEvent UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API GetRenderEvent_RenderEnd()
{
	return [](int frameID) { if (s_CurrentAPI != NULL) s_CurrentAPI->ReflexCallback_RenderEnd(frameID); };
}

extern "C" UnityRenderingEvent UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API GetRenderEvent_SetConstant()
{
	return [](int frameID) { if (s_CurrentAPI != NULL) s_CurrentAPI->ReflexCallback_RenderEnd(frameID); };
}

extern "C" UnityRenderingEvent UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API GetRenderEvent_SetTexture()
{

}

extern "C" UnityRenderingEvent UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API GetRenderEvent_Sleep()
{
	return [](int frameID) { if (s_CurrentAPI != NULL) s_CurrentAPI->ReflexCallback_Sleep(frameID); };
}

extern "C" UnityRenderingEvent UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API GetRenderEventFunc()
{
	return OnRenderEvent;
}

extern "C" UnityRenderingEventAndData UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API GetRenderEventWithDataFunc()
{
	return OnRenderEventWithData;
}

// --------------------------------------------------------------------------
// extern function
extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API SimulateBegin(int frameID)
{
	if (s_CurrentAPI != NULL) s_CurrentAPI->ReflexCallback_SimStart(frameID);
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API SimulateEnd(int frameID)
{
	if (s_CurrentAPI != NULL) s_CurrentAPI->ReflexCallback_SimEnd(frameID);
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
