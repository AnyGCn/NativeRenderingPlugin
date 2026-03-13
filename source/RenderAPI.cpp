#include "PlatformBase.h"
#include "RenderAPI.h"
#include "Unity/IUnityGraphics.h"

#if UNITY_WIN
#include "SLWrapper.h"
#endif

RenderAPI* CreateRenderAPI(UnityGfxRenderer apiType)
{
#	if SUPPORT_OPENGL_UNIFIED
	if (apiType == kUnityGfxRendererOpenGLCore || apiType == kUnityGfxRendererOpenGLES30)
	{
		extern RenderAPI* CreateRenderAPI_OpenGLCoreES(UnityGfxRenderer apiType);
		return CreateRenderAPI_OpenGLCoreES(apiType);
	}
#	endif // if SUPPORT_OPENGL_UNIFIED

#	if SUPPORT_METAL
	if (apiType == kUnityGfxRendererMetal)
	{
		extern RenderAPI* CreateRenderAPI_Metal();
		return CreateRenderAPI_Metal();
	}
#	endif // if SUPPORT_METAL

#	if SUPPORT_D3D11
	if (apiType == kUnityGfxRendererD3D11)
	{
		extern RenderAPI* CreateRenderAPI_D3D11();
		return CreateRenderAPI_D3D11();
	}
#	endif // if SUPPORT_D3D11

#	if SUPPORT_D3D12
	if (apiType == kUnityGfxRendererD3D12)
	{
		extern RenderAPI* CreateRenderAPI_D3D12();
		return CreateRenderAPI_D3D12();
	}
#	endif // if SUPPORT_D3D12

	// Unknown or unsupported graphics API
	return NULL;
}

void RenderAPI_OnPluginLoad()
{
#if SUPPORT_VULKAN
	extern void RenderAPI_Vulkan_OnPluginLoad(IUnityInterfaces*);
	RenderAPI_Vulkan_OnPluginLoad(unityInterfaces);
#endif // SUPPORT_VULKAN

#if SUPPORT_D3D11 || SUPPORT_D3D12
	SLWrapper::Get().Initialize_preDevice();
#endif
}

#if SUPPORT_D3D11 || SUPPORT_D3D12
void RenderAPI::ReflexCallback_Sleep(uint32_t frameID)
{
	SLWrapper::Get().ReflexCallback_Sleep(frameID);
}

void RenderAPI::ReflexCallback_SimStart(uint32_t frameID)
{
	SLWrapper::Get().ReflexCallback_SimStart(frameID);
}

void RenderAPI::ReflexCallback_SimEnd(uint32_t frameID)
{
	SLWrapper::Get().ReflexCallback_SimEnd(frameID);
}

void RenderAPI::ReflexCallback_RenderStart(uint32_t frameID)
{
	SLWrapper::Get().ReflexCallback_RenderStart(frameID);
}

void RenderAPI::ReflexCallback_RenderEnd(uint32_t frameID)
{
	SLWrapper::Get().ReflexCallback_RenderEnd(frameID);
}
#else
void RenderAPI::ReflexCallback_Sleep(uint32_t frameID)
{

}

void RenderAPI::ReflexCallback_SimStart(uint32_t frameID)
{

}

void RenderAPI::ReflexCallback_SimEnd(uint32_t frameID)
{

}

void RenderAPI::ReflexCallback_RenderStart(uint32_t frameID)
{

}

void RenderAPI::ReflexCallback_RenderEnd(uint32_t frameID)
{

}
#endif