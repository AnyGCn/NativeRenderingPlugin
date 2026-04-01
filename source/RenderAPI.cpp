#include "PlatformBase.h"
#include "RenderAPI.h"

#include <cstdarg>
#include <cstdio>
#include <cstdlib>

#include "Unity/IUnityGraphics.h"

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

static constexpr size_t g_MessageBufferSize = 4096;
void RenderAPI::LogInfo(const char* fmt...)
{
	char buffer[g_MessageBufferSize];
	va_list args;
	va_start(args, fmt);
	vsnprintf(buffer, sizeof(buffer), fmt, args);

	UNITY_LOG(RenderAPI::s_Logger, buffer);

	va_end(args);
}

void RenderAPI::LogWarning(const char* fmt...)
{
	char buffer[g_MessageBufferSize];
	va_list args;
	va_start(args, fmt);
	vsnprintf(buffer, sizeof(buffer), fmt, args);

	UNITY_LOG_WARNING(RenderAPI::s_Logger, buffer);

	va_end(args);
}

void RenderAPI::LogError(const char* fmt...)
{
	char buffer[g_MessageBufferSize];
	va_list args;
	va_start(args, fmt);
	vsnprintf(buffer, sizeof(buffer), fmt, args);

	UNITY_LOG_ERROR(RenderAPI::s_Logger, buffer);

	va_end(args);
}

void RenderAPI::LogFatal(const char* fmt...)
{
	char buffer[g_MessageBufferSize];
	va_list args;
	va_start(args, fmt);
	vsnprintf(buffer, sizeof(buffer), fmt, args);

	UNITY_LOG_ERROR(RenderAPI::s_Logger, buffer);
	abort();

	va_end(args);
}
