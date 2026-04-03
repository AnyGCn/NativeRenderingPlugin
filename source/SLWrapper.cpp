//----------------------------------------------------------------------------------
// File:        SLWrapper.cpp
// SDK Version: 2.0
// Email:       StreamlineSupport@nvidia.com
// Site:        http://developer.nvidia.com/
//
// Copyright (c) 2022-2025, NVIDIA CORPORATION. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
//  * Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//  * Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//  * Neither the name of NVIDIA CORPORATION nor the names of its
//    contributors may be used to endorse or promote products derived
//    from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
// OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
//----------------------------------------------------------------------------------
#include "SLWrapper.h"

#include <filesystem>
#include <dxgi.h>
#include <dxgi1_5.h>

#include <d3d11.h>
#include <d3d12.h>

#include "sl_security.h"

#include <cstdio>
#include <climits>

#include "RenderAPI_D3D.h"
#include "PlatformBase.h"

void logFunctionCallback(sl::LogType type, const char* msg)
{
    if (type == sl::LogType::eError)
    {
        // Add a breakpoint here to break on errors
        UNITY_LOG_ERROR(RenderAPI::s_Logger, msg);
    }
    if (type == sl::LogType::eWarn)
    {
        // Add a breakpoint here to break on warnings
        UNITY_LOG_WARNING(RenderAPI::s_Logger, msg);
    }
    else
    {
        UNITY_LOG(RenderAPI::s_Logger, msg);
    }
}

static const std::map<const sl::Result, const std::string> errors = {
    {sl::Result::eErrorIO, "eErrorIO"},
    {sl::Result::eErrorDriverOutOfDate, "eErrorDriverOutOfDate"},
    {sl::Result::eErrorOSOutOfDate, "eErrorOSOutOfDate"},
    {sl::Result::eErrorOSDisabledHWS, "eErrorOSDisabledHWS"},
    {sl::Result::eErrorDeviceNotCreated, "eErrorDeviceNotCreated"},
    {sl::Result::eErrorAdapterNotSupported, "eErrorAdapterNotSupported"},
    {sl::Result::eErrorNoPlugins, "eErrorNoPlugins"},
    {sl::Result::eErrorVulkanAPI, "eErrorVulkanAPI"},
    {sl::Result::eErrorDXGIAPI, "eErrorDXGIAPI"},
    {sl::Result::eErrorD3DAPI, "eErrorD3DAPI"},
    {sl::Result::eErrorNRDAPI, "eErrorNRDAPI"},
    {sl::Result::eErrorNVAPI, "eErrorNVAPI"},
    {sl::Result::eErrorReflexAPI, "eErrorReflexAPI"},
    {sl::Result::eErrorNGXFailed, "eErrorNGXFailed"},
    {sl::Result::eErrorJSONParsing, "eErrorJSONParsing"},
    {sl::Result::eErrorMissingProxy, "eErrorMissingProxy"},
    {sl::Result::eErrorMissingResourceState, "eErrorMissingResourceState"},
    {sl::Result::eErrorInvalidIntegration, "eErrorInvalidIntegration"},
    {sl::Result::eErrorMissingInputParameter, "eErrorMissingInputParameter"},
    {sl::Result::eErrorNotInitialized, "eErrorNotInitialized"},
    {sl::Result::eErrorComputeFailed, "eErrorComputeFailed"},
    {sl::Result::eErrorInitNotCalled, "eErrorInitNotCalled"},
    {sl::Result::eErrorExceptionHandler, "eErrorExceptionHandler"},
    {sl::Result::eErrorInvalidParameter, "eErrorInvalidParameter"},
    {sl::Result::eErrorMissingConstants, "eErrorMissingConstants"},
    {sl::Result::eErrorDuplicatedConstants, "eErrorDuplicatedConstants"},
    {sl::Result::eErrorMissingOrInvalidAPI, "eErrorMissingOrInvalidAPI"},
    {sl::Result::eErrorCommonConstantsMissing, "eErrorCommonConstantsMissing"},
    {sl::Result::eErrorUnsupportedInterface, "eErrorUnsupportedInterface"},
    {sl::Result::eErrorFeatureMissing, "eErrorFeatureMissing"},
    {sl::Result::eErrorFeatureNotSupported, "eErrorFeatureNotSupported"},
    {sl::Result::eErrorFeatureMissingHooks, "eErrorFeatureMissingHooks"},
    {sl::Result::eErrorFeatureFailedToLoad, "eErrorFeatureFailedToLoad"},
    {sl::Result::eErrorFeatureWrongPriority, "eErrorFeatureWrongPriority"},
    {sl::Result::eErrorFeatureMissingDependency, "eErrorFeatureMissingDependency"},
    {sl::Result::eErrorFeatureManagerInvalidState, "eErrorFeatureManagerInvalidState"},
    {sl::Result::eErrorInvalidState, "eErrorInvalidState"},
    {sl::Result::eWarnOutOfVRAM, "eWarnOutOfVRAM"}
};

bool successCheck(sl::Result result, const char* location)
{
    if (result == sl::Result::eOk)
        return true;

    auto a = errors.find(result);
    if (a != errors.end())
        logFunctionCallback(sl::LogType::eError,
                            ("Error: " + a->second + (location == nullptr
                                                          ? ""
                                                          : (" encountered in " + std::string(location)))).c_str());
    else
        logFunctionCallback(sl::LogType::eError,
                            ("Unknown error " + static_cast<int>(result) + (location == nullptr
                                                                                ? ""
                                                                                : (" encountered in " + std::string(
                                                                                    location)))).c_str());

    return false;
}

std::wstring GetSlInterposerDllLocation()
{
    wchar_t path[MAX_PATH] = {0};
    HMODULE thisModule = GetModuleHandleW(L"RenderingPlugin.dll");
#ifdef _WIN32
    if (GetModuleFileNameW(thisModule, path, MAX_PATH) == 0)
        return std::wstring();
#else // _WIN32
#error Unsupported platform for GetSlInterposerDllLocation!
#endif // _WIN32

    auto basePath = std::filesystem::path(path).parent_path();
    auto dllPath = basePath.wstring().append(L"\\sl.interposer.dll");
    return dllPath;
}

SLWrapper& SLWrapper::Get()
{
    static SLWrapper instance;
    return instance;
}

bool SLWrapper::Initialize_preDevice()
{
    if (m_sl_initialised)
    {
        UNITY_LOG(RenderAPI::s_Logger, "SLWrapper is already initialised.");
        return true;
    }

    sl::Preferences pref;

    pref.allocateCallback = &allocateResourceCallback;
    pref.releaseCallback = &releaseResourceCallback;
    pref.applicationId = APP_ID;

#if _DEBUG
    pref.showConsole = false;
    pref.logMessageCallback = &logFunctionCallback;
    pref.logLevel = sl::LogLevel::eDefault;
#else
    pref.showConsole = false;
    pref.logMessageCallback = &logFunctionCallback;
    pref.logLevel = sl::LogLevel::eDefault;
#endif

    sl::Feature featuresToLoad[] = {
        sl::kFeatureDLSS,
        sl::kFeatureDLSS_G,
        sl::kFeatureReflex,
        // PCL is always implicitly loaded, but request it to ensure we never have 0-sized array
        sl::kFeaturePCL
    };
    pref.featuresToLoad = featuresToLoad;
    pref.numFeaturesToLoad = static_cast<uint32_t>(std::size(featuresToLoad));
    pref.flags = sl::PreferenceFlags::eDisableCLStateTracking;
    // pref.flags |= sl::PreferenceFlags::eUseManualHooking;
    pref.flags |= sl::PreferenceFlags::eUseFrameBasedResourceTagging;

    auto pathDll = GetSlInterposerDllLocation();

    HMODULE interposer = {};
    if (sl::security::verifyEmbeddedSignature(pathDll.c_str()))
    {
        interposer = LoadLibraryW(pathDll.c_str());
    }
    else
    {
        interposer = LoadLibraryW(pathDll.c_str());
    }

    if (!interposer)
    {
        RenderAPI::LogError("Unable to load Streamline Interposer");
        return false;
    }

    m_sl_initialised = successCheck(slInit(pref, SDK_VERSION), "slInit");
    if (!m_sl_initialised)
    {
        RenderAPI::LogError("Failed to initialse SL.");
        return false;
    }

    return true;
}

bool SLWrapper::Initialize(UnityGfxRenderer api, RenderAPI_D3D* renderApi)
{
    // If Initialize_preDevice has not been call before manually, call it now
    Initialize_preDevice();
    m_api = api;
    m_renderAPI = renderApi;
    SetDevice();

    // We set reflex consts to a default config. This can be changed at runtime in the UI.
    auto reflexConst = sl::ReflexOptions{};
    reflexConst.mode = sl::ReflexMode::eOff;
    reflexConst.useMarkersToOptimize = false; // Not supported on single stage engine.
    reflexConst.virtualKey = VK_F13;
    reflexConst.frameLimitUs = 0;
    SetReflexConsts(reflexConst);
    UpdateFeatureAvailable();
    slGetNewFrameToken(m_currentSimFrame, &m_currentSimFrameID);
    slGetNewFrameToken(m_currentRenderFrame, &m_currentRenderFrameID);

    return true;
}

sl::FeatureRequirements SLWrapper::GetFeatureRequirements(sl::Feature feature)
{
    sl::FeatureRequirements req;
    slGetFeatureRequirements(feature, req);
    return req;
}

sl::FeatureVersion SLWrapper::GetFeatureVersion(sl::Feature feature)
{
    sl::FeatureVersion ver;
    slGetFeatureVersion(feature, ver);
    return ver;
}

void SLWrapper::SetDevice()
{
    successCheck(slSetD3DDevice(m_renderAPI->GetDevice()), "slSetD3DDevice");
}

void SLWrapper::UpdateFeatureAvailable()
{
    LUID luid = m_renderAPI->GetAdapterLuid();
    sl::AdapterInfo adapterInfo;
    adapterInfo.deviceLUID = (uint8_t*)&luid;
    adapterInfo.deviceLUIDSizeInBytes = sizeof(LUID);

    // Check if features are fully functional (2nd call of slIsFeatureSupported onwards)
    sl::FeatureRequirements dlss_requirements;
    slGetFeatureRequirements(sl::kFeatureDLSS, dlss_requirements);

    m_dlss_available = slIsFeatureSupported(sl::kFeatureDLSS, adapterInfo) == sl::Result::eOk;
    if (m_dlss_available) RenderAPI::LogInfo("DLSS is supported on this system.");
    else RenderAPI::LogWarning("DLSS is not fully functional on this system.");

    sl::FeatureRequirements dlssg_requirements;
    slGetFeatureRequirements(sl::kFeatureDLSS_G, dlssg_requirements);

    m_dlssg_available = slIsFeatureSupported(sl::kFeatureDLSS_G, adapterInfo) == sl::Result::eOk;
    if (m_dlssg_available) RenderAPI::LogInfo("DLSS-G is supported on this system.");
    else RenderAPI::LogWarning("DLSS-G is not fully functional on this system.");

    m_pcl_available = successCheck(slIsFeatureSupported(sl::kFeaturePCL, adapterInfo), "slIsFeatureSupported_PCL");
    if (m_pcl_available) RenderAPI::LogInfo("PCL is supported on this system.");
    else RenderAPI::LogWarning("PCL is not fully functional on this system.");

    sl::FeatureRequirements reflex_requirements;
    slGetFeatureRequirements(sl::kFeatureReflex, reflex_requirements);

    m_reflex_available = slIsFeatureSupported(sl::kFeatureReflex, adapterInfo) == sl::Result::eOk;
    if (m_reflex_available) RenderAPI::LogInfo("Reflex is supported on this system.");
    else RenderAPI::LogWarning("Reflex is not fully functional on this system.");
}

void SLWrapper::Shutdown()
{
    // Un-set all tags
    sl::ResourceTag inputs[] = {
        sl::ResourceTag{nullptr, sl::kBufferTypeDepth, sl::ResourceLifecycle::eValidUntilPresent},
        sl::ResourceTag{nullptr, sl::kBufferTypeBackbuffer, sl::ResourceLifecycle::eValidUntilPresent},
        sl::ResourceTag{nullptr, sl::kBufferTypeMotionVectors, sl::ResourceLifecycle::eValidUntilPresent},
        sl::ResourceTag{nullptr, sl::kBufferTypeScalingInputColor, sl::ResourceLifecycle::eValidUntilPresent},
        sl::ResourceTag{nullptr, sl::kBufferTypeScalingOutputColor, sl::ResourceLifecycle::eValidUntilPresent},
        sl::ResourceTag{nullptr, sl::kBufferTypeHUDLessColor, sl::ResourceLifecycle::eValidUntilPresent}
    };
    successCheck(SetTag(inputs, _countof(inputs), nullptr), "slSetTag_clear");

    // Shutdown Streamline
    if (m_sl_initialised)
    {
        successCheck(slShutdown(), "slShutdown");
        m_sl_initialised = false;
    }
}

void SLWrapper::SetSLConsts(const CameraData& cameraData)
{
    if (!m_sl_initialised)
    {
        RenderAPI::LogWarning("SL not initialised.");
        return;
    }

    // 初始化RenderSurfaceSettings
    m_renderSizeX = cameraData.inputSize[0];
    m_renderSizeY = cameraData.inputSize[1];
    m_outputSizeX = cameraData.outputSize[0];
    m_outputSizeY = cameraData.outputSize[1];
    
    // 初始化sl::Constants
    sl::Constants slConstants{};
    slConstants.cameraViewToClip = *reinterpret_cast<const sl::float4x4*>(cameraData.cameraViewToClip);
    slConstants.clipToCameraView = *reinterpret_cast<const sl::float4x4*>(cameraData.clipToCameraView);
    slConstants.clipToLensClip = sl::float4x4{};
    slConstants.clipToPrevClip = *reinterpret_cast<const sl::float4x4*>(cameraData.clipToPrevClip);
    slConstants.prevClipToClip = *reinterpret_cast<const sl::float4x4*>(cameraData.prevClipToClip);
    slConstants.jitterOffset = sl::float2{ cameraData.jitterOffset[0], cameraData.jitterOffset[1] };
    slConstants.mvecScale = sl::float2{ cameraData.mvecScale[0], cameraData.mvecScale[1] };
    slConstants.cameraPinholeOffset = sl::float2{ 0, 0 };
    slConstants.cameraPos = sl::float3{ cameraData.cameraPos[0], cameraData.cameraPos[1], cameraData.cameraPos[2] };
    slConstants.cameraUp = sl::float3{ cameraData.cameraUp[0], cameraData.cameraUp[1], cameraData.cameraUp[2] };
    slConstants.cameraRight = sl::float3{ cameraData.cameraRight[0], cameraData.cameraRight[1], cameraData.cameraRight[2] };
    slConstants.cameraFwd = sl::float3{ cameraData.cameraFwd[0], cameraData.cameraFwd[1], cameraData.cameraFwd[2] };
    slConstants.cameraNear = cameraData.cameraNear;
    slConstants.cameraFar = cameraData.cameraFar;
    slConstants.cameraFOV = cameraData.cameraFOV;
    slConstants.cameraAspectRatio = 1.0f * cameraData.outputSize[0] / static_cast<float>(cameraData.outputSize[1]);
    slConstants.motionVectorsInvalidValue = 0.0f;
    slConstants.depthInverted = cameraData.depthInverted ? sl::Boolean::eTrue : sl::Boolean::eFalse;
    slConstants.cameraMotionIncluded = sl::Boolean::eTrue;
    slConstants.motionVectors3D = sl::Boolean::eFalse;
    slConstants.reset = cameraData.reset ? sl::Boolean::eTrue : sl::Boolean::eFalse;
    slConstants.orthographicProjection = sl::Boolean::eFalse;
    slConstants.motionVectorsDilated = sl::Boolean::eFalse;
    slConstants.motionVectorsJittered = sl::Boolean::eFalse;
    successCheck(slSetConstants(slConstants, *m_currentSimFrame, m_viewport), "slSetConstants");
}

void SLWrapper::FeatureLoad(sl::Feature feature, const bool turn_on)
{
    if (m_api == UnityGfxRenderer::kUnityGfxRendererD3D12)
    {
        bool loaded;
        slIsFeatureLoaded(feature, loaded);
        if (loaded ^ turn_on)
        {
            slSetFeatureLoaded(feature, turn_on);
        }
    }
}

void SLWrapper::SetDLSSOptions(const sl::DLSSOptions consts)
{
    if (!m_sl_initialised || !m_dlss_available)
    {
        RenderAPI::LogWarning("SL not initialised or DLSS not available.");
        return;
    }

    m_dlss_consts = consts;
    successCheck(slDLSSSetOptions(m_viewport, m_dlss_consts), "slDLSSSetOptions");
}

DLSSSettings SLWrapper::QueryDLSSOptimalSettings(const sl::DLSSOptions& consts)
{
    DLSSSettings settings{};
    if (!m_sl_initialised || !m_dlss_available)
    {
        RenderAPI::LogWarning("SL not initialised or DLSS not available.");
        return settings;
    }

    sl::DLSSOptimalSettings dlssOptimal = {};
    successCheck(slDLSSGetOptimalSettings(consts, dlssOptimal), "slDLSSGetOptimalSettings");

    settings.optimalRenderSizeX = static_cast<int>(dlssOptimal.optimalRenderWidth);
    settings.optimalRenderSizeY = static_cast<int>(dlssOptimal.optimalRenderHeight);
    settings.sharpness = dlssOptimal.optimalSharpness;

    settings.minRenderSizeX = static_cast<int>(dlssOptimal.renderWidthMin);
    settings.minRenderSizeY = static_cast<int>(dlssOptimal.renderHeightMin);
    settings.maxRenderSizeX = static_cast<int>(dlssOptimal.renderWidthMax);
    settings.maxRenderSizeY = static_cast<int>(dlssOptimal.renderHeightMax);
    return settings;
}

void SLWrapper::CleanupDLSS(bool wfi)
{
    if (!m_sl_initialised)
    {
        RenderAPI::LogWarning("SL not initialised.");
        return;
    }
    if (!m_dlss_available)
    {
        return;
    }

    if (!m_dlss_available)
    {
        RenderAPI::LogWarning("DLSS not available.");
        return;
    }

    if (wfi)
    {
        // m_Device->waitForIdle();
    }

    sl::Result status = slFreeResources(sl::kFeatureDLSS, m_viewport);
    // if we've never ran the feature on this viewport, this call may return 'eErrorInvalidParameter'
    assert(status == sl::Result::eOk || status == sl::Result::eErrorInvalidParameter);
}

void SLWrapper::SetDLSSGOptions(const sl::DLSSGOptions consts)
{
    if (!m_sl_initialised || !m_dlssg_available)
    {
        RenderAPI::LogWarning("SL not initialised or DLSSG not available.");
        return;
    }

    m_dlssg_consts = consts;
    successCheck(slDLSSGSetOptions(m_viewport, m_dlssg_consts), "slDLSSGSetOptions");
}

void SLWrapper::QueryDLSSGState(uint64_t& estimatedVRamUsage, int& fps_multiplier, sl::DLSSGStatus& status,
                                int& minSize, int& maxFrameCount, void*& pFence, uint64_t& fenceValue)
{
    if (!m_sl_initialised || !m_dlssg_available)
    {
        RenderAPI::LogWarning("SL not initialised or DLSSG not available.");
        return;
    }

    successCheck(slDLSSGGetState(m_viewport, m_dlssg_settings, &m_dlssg_consts), "slDLSSGGetState");

    estimatedVRamUsage = m_dlssg_settings.estimatedVRAMUsageInBytes;
    fps_multiplier = m_dlssg_settings.numFramesActuallyPresented;
    status = m_dlssg_settings.status;
    minSize = m_dlssg_settings.minWidthOrHeight;
    maxFrameCount = m_dlssg_settings.numFramesToGenerateMax;
    pFence = m_dlssg_settings.inputsProcessingCompletionFence;
    fenceValue = m_dlssg_settings.lastPresentInputsProcessingCompletionFenceValue;
}

uint64_t SLWrapper::GetDLSSGLastFenceValue()
{
    return m_dlssg_settings.lastPresentInputsProcessingCompletionFenceValue;
}

bool SLWrapper::Get_DLSSG_SwapChainRecreation(bool& turn_on) const
{
    turn_on = m_dlssg_shoudLoad;
    auto tmp = m_dlssg_triggerswapchainRecreation;
    return tmp;
}

void SLWrapper::CleanupDLSSG(bool wfi)
{
    if (!m_sl_initialised)
    {
        RenderAPI::LogWarning("SL not initialised.");
        return;
    }

    if (!m_dlssg_available)
    {
        RenderAPI::LogWarning("DLSSG not available.");
        return;
    }

    if (wfi)
    {
        // m_Device->waitForIdle();
    }

    sl::Result status = slFreeResources(sl::kFeatureDLSS_G, m_viewport);
    // if we've never ran the feature on this viewport, this call may return 'eErrorInvalidParameter'
    assert(
        status == sl::Result::eOk || status == sl::Result::eErrorInvalidParameter || status == sl::Result::
        eErrorFeatureMissing);
}

sl::Resource SLWrapper::allocateResourceCallback(const sl::ResourceAllocationDesc* resDesc, void* device)
{
    if (resDesc->type == sl::ResourceType::eBuffer)
    {
        return Get().m_renderAPI->AllocateBuffer(resDesc, device);
    }
    else
    {
        return Get().m_renderAPI->AllocateTexture(resDesc, device);
    }

    return {};
}

void SLWrapper::releaseResourceCallback(sl::Resource* resource, void* device)
{
    if (resource)
    {
        auto i = (IUnknown*)resource->native;
        i->Release();
    }
};

void SLWrapper::GetSLResource(sl::Resource& slResource, void* inputTex)
{
    // TODO: determine resource state manually
    slResource = sl::Resource{sl::ResourceType::eTex2d, inputTex, nullptr, nullptr, static_cast<uint32_t>(D3D12_RESOURCE_STATE_COMMON)};
}

void SLWrapper::TagResources_General(void* commandList, void* motionVectors, void* depth)
{
    if (!m_sl_initialised)
    {
        RenderAPI::LogWarning("Streamline not initialised.");
        return;
    }

    sl::Extent renderExtent{0, 0, m_renderSizeX, m_renderSizeY};
    sl::Resource depthResource(sl::ResourceType::eTex2d, depth, D3D12_RESOURCE_STATE_COMMON);
    sl::Resource motionVectorsResource(sl::ResourceType::eTex2d, motionVectors, D3D12_RESOURCE_STATE_COMMON);
    sl::ResourceTag motionVectorsResourceTag = sl::ResourceTag{&motionVectorsResource, sl::kBufferTypeMotionVectors, sl::ResourceLifecycle::eValidUntilPresent, &renderExtent};
    sl::ResourceTag depthResourceTag = sl::ResourceTag{&depthResource, sl::kBufferTypeDepth, sl::ResourceLifecycle::eValidUntilPresent, &renderExtent};
    sl::ResourceTag inputs[] = {motionVectorsResourceTag, depthResourceTag};
    successCheck(SetTag(inputs, _countof(inputs), commandList), "slSetTag_General");
}

void SLWrapper::TagResources_DLSS_NIS(void* commandList, void* Output, void* Input)
{
    if (!m_sl_initialised)
    {
        RenderAPI::LogWarning("Streamline not initialised.");
        return;
    }

    sl::Extent renderExtent{0, 0, m_renderSizeX, m_renderSizeY};
    sl::Extent fullExtent{0, 0, m_outputSizeX, m_outputSizeY};
    sl::Resource inputResource(sl::ResourceType::eTex2d, Input, D3D12_RESOURCE_STATE_COMMON);
    sl::Resource outputResource(sl::ResourceType::eTex2d, Output, D3D12_RESOURCE_STATE_COMMON);
    sl::ResourceTag inputResourceTag = sl::ResourceTag{&inputResource, sl::kBufferTypeScalingInputColor, sl::ResourceLifecycle::eValidUntilPresent, &renderExtent};
    sl::ResourceTag outputResourceTag = sl::ResourceTag{&outputResource, sl::kBufferTypeScalingOutputColor, sl::ResourceLifecycle::eValidUntilPresent, &fullExtent};
    sl::ResourceTag inputs[] = {inputResourceTag, outputResourceTag};
    successCheck(SetTag(inputs, _countof(inputs), commandList), "slSetTag_dlss_nis");
}

void SLWrapper::TagResources_DLSS_FG(void* commandList, void* finalColorHudless)
{
    if (!m_sl_initialised)
    {
        RenderAPI::LogWarning("Streamline not initialised.");
        return;
    }

    sl::Extent fullExtent{0, 0, m_outputSizeX, m_outputSizeY};
    sl::Resource hudlessResource(sl::ResourceType::eTex2d, finalColorHudless, D3D12_RESOURCE_STATE_COMMON);
    sl::ResourceTag hudlessResourceTag = sl::ResourceTag{&hudlessResource, sl::kBufferTypeHUDLessColor, sl::ResourceLifecycle::eValidUntilPresent, &fullExtent};
    // tag backbuffer resource mainly to pass extent data and therefore resource can be nullptr.
    // If the viewport extent is invalid - set extent to null. This informs streamline that full resource extent needs to be used
    sl::ResourceTag backBufferResourceTag = sl::ResourceTag{nullptr, sl::kBufferTypeBackbuffer, sl::ResourceLifecycle{}, &fullExtent};
    sl::ResourceTag inputs[] = {backBufferResourceTag, hudlessResourceTag};
    successCheck(SetTag(inputs, _countof(inputs), commandList), "slSetTag_dlss_fg");
}

void SLWrapper::EvaluateDLSS(void* commandList)
{
    sl::ViewportHandle view(m_viewport);
    const sl::BaseStructure* inputs[] = {&view};
    successCheck(slDLSSSetOptions(m_viewport, m_dlss_consts), "slDLSSSetOptions");
    successCheck(slEvaluateFeature(sl::kFeatureDLSS, *m_currentRenderFrame, inputs, _countof(inputs), commandList),
                 "slEvaluateFeature_DLSS");
}

void SLWrapper::SetReflexConsts(const sl::ReflexOptions options)
{
    if (!m_sl_initialised || !m_reflex_available)
    {
        RenderAPI::LogWarning("SL not initialised or Reflex not available.");
        return;
    }

    m_reflex_consts = options;
    successCheck(slReflexSetOptions(m_reflex_consts), "Reflex_Options");

    return;
}

void SLWrapper::ReflexCallback_Sleep(uint32_t frameID)
{
    m_currentSimFrameID = frameID;
    successCheck(slGetNewFrameToken(m_currentSimFrame, &m_currentSimFrameID), "SL_GetFrameToken");
    if (SLWrapper::Get().GetReflexAvailable())
    {
        successCheck(slReflexSleep(*m_currentSimFrame), "Reflex_Sleep");
    }
}

void SLWrapper::ReflexCallback_SimStart(uint32_t frameID)
{
    assert(frameID == m_currentSimFrameID);
    if (SLWrapper::Get().GetPCLAvailable())
    {
        successCheck(slPCLSetMarker(sl::PCLMarker::eSimulationStart, *m_currentSimFrame), "PCL_SimStart");
    }
}

void SLWrapper::ReflexCallback_SimEnd(uint32_t frameID)
{
    assert(frameID == m_currentSimFrameID);
    if (SLWrapper::Get().GetPCLAvailable())
    {
        successCheck(slPCLSetMarker(sl::PCLMarker::eSimulationEnd, *m_currentSimFrame), "PCL_SimEnd");
    }
}

void SLWrapper::ReflexCallback_RenderStart(uint32_t frameID)
{
    m_currentRenderFrameID = frameID;
    successCheck(slGetNewFrameToken(m_currentRenderFrame, &m_currentRenderFrameID), "SL_GetFrameToken");
    if (SLWrapper::Get().GetPCLAvailable())
    {
        successCheck(slPCLSetMarker(sl::PCLMarker::eRenderSubmitStart, *m_currentRenderFrame), "PCL_SubmitStart");
    }
}

void SLWrapper::ReflexCallback_RenderEnd(uint32_t frameID)
{
    assert(frameID == m_currentRenderFrameID);
    if (SLWrapper::Get().GetPCLAvailable())
    {
        successCheck(slPCLSetMarker(sl::PCLMarker::eRenderSubmitEnd, *m_currentRenderFrame), "PCL_SubmitEnd");
    }
}

void SLWrapper::ReflexCallback_PresentStart()
{
    if (SLWrapper::Get().GetPCLAvailable())
    {
        successCheck(slPCLSetMarker(sl::PCLMarker::ePresentStart, *m_currentRenderFrame), "PCL_PresentStart");
    }
}

void SLWrapper::ReflexCallback_PresentEnd()
{
    if (SLWrapper::Get().GetPCLAvailable())
    {
        successCheck(slPCLSetMarker(sl::PCLMarker::ePresentEnd, *m_currentRenderFrame), "PCL_PresentEnd");
    }
}

void SLWrapper::ReflexTriggerFlash()
{
    successCheck(slPCLSetMarker(sl::PCLMarker::eTriggerFlash, *SLWrapper::Get().m_currentRenderFrame), "Reflex_Flash");
}

void SLWrapper::ReflexTriggerPcPing()
{
    if (SLWrapper::Get().GetPCLAvailable())
    {
        successCheck(slPCLSetMarker(sl::PCLMarker::ePCLatencyPing, *SLWrapper::Get().m_currentRenderFrame), "PCL_PCPing");
    }
}

void SLWrapper::QueryReflexStats(bool& reflex_lowLatencyAvailable, bool& reflex_flashAvailable, std::string& stats)
{
    if (SLWrapper::GetReflexAvailable())
    {
        sl::ReflexState state;
        successCheck(slReflexGetState(state), "Reflex_State");

        reflex_lowLatencyAvailable = state.lowLatencyAvailable;
        reflex_flashAvailable = state.flashIndicatorDriverControlled;

        auto rep = state.frameReport[63];
        if (state.latencyReportAvailable && rep.gpuRenderEndTime != 0)
        {
            auto frameID = rep.frameID;
            auto totalGameToRenderLatencyUs = rep.gpuRenderEndTime - rep.inputSampleTime;
            auto simDeltaUs = rep.simEndTime - rep.simStartTime;
            auto renderDeltaUs = rep.renderSubmitEndTime - rep.renderSubmitStartTime;
            auto presentDeltaUs = rep.presentEndTime - rep.presentStartTime;
            auto driverDeltaUs = rep.driverEndTime - rep.driverStartTime;
            auto osRenderQueueDeltaUs = rep.osRenderQueueEndTime - rep.osRenderQueueStartTime;
            auto gpuRenderDeltaUs = rep.gpuRenderEndTime - rep.gpuRenderStartTime;

            stats = "frameID: " + std::to_string(frameID);
            stats += "\ntotalGameToRenderLatencyUs: " + std::to_string(totalGameToRenderLatencyUs);
            stats += "\nsimDeltaUs: " + std::to_string(simDeltaUs);
            stats += "\nrenderDeltaUs: " + std::to_string(renderDeltaUs);
            stats += "\npresentDeltaUs: " + std::to_string(presentDeltaUs);
            stats += "\ndriverDeltaUs: " + std::to_string(driverDeltaUs);
            stats += "\nosRenderQueueDeltaUs: " + std::to_string(osRenderQueueDeltaUs);
            stats += "\ngpuRenderDeltaUs: " + std::to_string(gpuRenderDeltaUs);
        }
        else
        {
            stats = "Latency Report Unavailable";
        }
    }
}

void SLWrapper::SetReflexCameraData(sl::FrameToken& frameToken, const sl::ReflexCameraData& cameraData)
{
    slReflexSetCameraData(m_viewport, frameToken, cameraData);
}
