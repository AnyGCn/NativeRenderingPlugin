#include "RenderAPI.h"
#include "PlatformBase.h"

// OpenGL Core profile (desktop) or OpenGL ES (mobile) implementation of RenderAPI.
// Supports several flavors: Core, ES2, ES3


#if SUPPORT_OPENGL_UNIFIED


#include <assert.h>
#include <string>
#include <EGL/egl.h>
#include <GLES3/gl32.h>
#include <GLES3/gl3ext.h>

typedef void (GL_APIENTRYP PFNGLQCOMFRAMEEXTRAPOLATION) (GLuint src1, GLuint src2, GLuint output, float scaleFactor);

class RenderAPI_OpenGLCoreES : public RenderAPI
{
public:
	RenderAPI_OpenGLCoreES(UnityGfxRenderer apiType);
	virtual ~RenderAPI_OpenGLCoreES() { }

	virtual void ProcessDeviceEvent(UnityGfxDeviceEventType type, IUnityInterfaces* interfaces);
	virtual void FrameExtrapolate(void* data);
	virtual bool SupportFrameExtrapolate() { return m_SupportFrameExtrapolation; }

private:
	void CreateResources();

private:
	UnityGfxRenderer m_APIType;
	PFNGLQCOMFRAMEEXTRAPOLATION m_glExtrapolateTex2DQCOM;
	bool m_SupportFrameExtrapolation;
};


RenderAPI* CreateRenderAPI_OpenGLCoreES(UnityGfxRenderer apiType)
{
	return new RenderAPI_OpenGLCoreES(apiType);
}

bool FrmGLExtensionSupported(const char*const extensionName)
{
	GLint extensionsNum = 0;
	glGetIntegerv(GL_NUM_EXTENSIONS, &extensionsNum);
	for (GLint i = 0; i < extensionsNum; ++i)
	{
		if(strncmp(extensionName, reinterpret_cast<const char*>(glGetStringi(GL_EXTENSIONS, static_cast<GLuint>(i))), 1 << 8) == 0)
		{
			return true;
		}
	}
	return false;
}

void RenderAPI_OpenGLCoreES::CreateResources()
{
#	if UNITY_WIN && SUPPORT_OPENGL_CORE
	if (m_APIType == kUnityGfxRendererOpenGLCore)
		gl3wInit();
#	endif
	// Make sure that there are no GL error flags set before creating resources
	while (glGetError() != GL_NO_ERROR) {}

	const char*const GL_QCOM_frame_extrapolation_cStr = "GL_QCOM_frame_extrapolation";
	m_SupportFrameExtrapolation = FrmGLExtensionSupported(GL_QCOM_frame_extrapolation_cStr);
	if(m_SupportFrameExtrapolation)
		m_glExtrapolateTex2DQCOM = (PFNGLQCOMFRAMEEXTRAPOLATION)eglGetProcAddress("glExtrapolateTex2DQCOM");
}

RenderAPI_OpenGLCoreES::RenderAPI_OpenGLCoreES(UnityGfxRenderer apiType)
	: m_APIType(apiType),
	m_glExtrapolateTex2DQCOM(NULL),
	m_SupportFrameExtrapolation(false)
{
}

void RenderAPI_OpenGLCoreES::ProcessDeviceEvent(UnityGfxDeviceEventType type, IUnityInterfaces* interfaces)
{
	if (type == kUnityGfxDeviceEventInitialize)
	{
		CreateResources();
	}
	else if (type == kUnityGfxDeviceEventShutdown)
	{
		//@TODO: release resources
	}
}

struct FrameExtrapolateData
{
	void* src1;
	void* src2;
	void* dst;
	float scaleFactor;
};

void RenderAPI_OpenGLCoreES::FrameExtrapolate(void* data)
{
	FrameExtrapolateData* frameExtrapolateData = (FrameExtrapolateData*)data;
	GLuint src1 = (GLuint)(size_t)(frameExtrapolateData->src1);
	GLuint src2 = (GLuint)(size_t)(frameExtrapolateData->src2);
	GLuint dst = (GLuint)(size_t)(frameExtrapolateData->dst);
	m_glExtrapolateTex2DQCOM(src1, src2, dst, frameExtrapolateData->scaleFactor);
}

#endif // #if SUPPORT_OPENGL_UNIFIED
