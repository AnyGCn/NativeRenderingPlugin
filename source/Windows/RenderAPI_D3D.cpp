#include "../PlatformBase.h"

#if SUPPORT_D3D11 || SUPPORT_D3D12

#include "RenderAPI_D3D.h"

#include "SLWrapper.h"

extern "C" void OnPluginLoad_D3D()
{
    SLWrapper::Get().Initialize_preDevice();
}

bool RenderAPI_D3D::SupportDLSS()
{
    return SLWrapper::Get().GetDLSSAvailable();
}

void RenderAPI_D3D::CleanupDLSS()
{
    SLWrapper::Get().CleanupDLSS(false);
}

DLSSSettings RenderAPI_D3D::QueryDLSSOptimalSettings(int outputSizeX, int outputSizeY, DLSSMode mode)
{
    sl::DLSSOptions dlssOptions;
    dlssOptions.mode = static_cast<sl::DLSSMode>(mode);
    dlssOptions.outputWidth = outputSizeX;
    dlssOptions.outputHeight = outputSizeY;
    return SLWrapper::Get().QueryDLSSOptimalSettings(dlssOptions);
}

void RenderAPI_D3D::UpscaleTextureDLSS(void* context) const
{
    sl::DLSSOptions dlssOptions;
    dlssOptions.mode = static_cast<sl::DLSSMode>(m_dlss_mode);
    dlssOptions.outputWidth = m_CameraData.outputSize.x;
    dlssOptions.outputHeight = m_CameraData.outputSize.y;
    dlssOptions.alphaUpscalingEnabled = sl::Boolean(m_CameraData.alphaUpscalingEnabled);
    dlssOptions.colorBuffersHDR = sl::Boolean(m_CameraData.colorBuffersHDR);
    SLWrapper::Get().SetViewportHandle(m_CameraData.viewHandle);
    SLWrapper::Get().SetSLConsts(m_CameraData);
    SLWrapper::Get().SetDLSSOptions(dlssOptions);
    SLWrapper::Get().TagResources_General(context, m_Textures[eMotionVectors], m_Textures[eDepth]);
    SLWrapper::Get().TagResources_DLSS_NIS(context, m_Textures[eScalingOutputColor], m_Textures[eScalingInputColor]);
    SLWrapper::Get().EvaluateDLSS(context);
}

void RenderAPI_D3D::ReflexCallback_Sleep(uint32_t frameID)
{
    // Cleanup status
    SLWrapper::Get().ReflexCallback_Sleep(frameID);
}

void RenderAPI_D3D::ReflexCallback_SimStart(uint32_t frameID)
{
    SLWrapper::Get().ReflexCallback_SimStart(frameID);
}

void RenderAPI_D3D::ReflexCallback_SimEnd(uint32_t frameID)
{
    SLWrapper::Get().ReflexCallback_SimEnd(frameID);
}

void RenderAPI_D3D::ReflexCallback_RenderStart(uint32_t frameID)
{
    SLWrapper::Get().ReflexCallback_RenderStart(frameID);
}

void RenderAPI_D3D::ReflexCallback_RenderEnd(uint32_t frameID)
{
    SLWrapper::Get().ReflexCallback_RenderEnd(frameID);
}

#endif
