//----------------------------------------------------------------------------------
// File:        SLWrapper.h
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
#pragma once

#include <map>
#include <sl.h>
#include <sl_consts.h>
#include <sl_hooks.h>
#include <sl_version.h>

// Streamline Features
#include <sl_dlss.h>
#include <sl_reflex.h>
#include <sl_dlss_g.h>

#include "Unity/IUnityGraphics.h"

struct CameraData;
struct DLSSSettings;

// Set this to a game's specific sdk version
static constexpr uint64_t SDK_VERSION = sl::kSDKVersion;
static constexpr int APP_ID = 231313132;

void logFunctionCallback(sl::LogType type, const char* msg);

bool successCheck(sl::Result result, const char* location = nullptr);

class SLWrapper
{
private:

    SLWrapper() = default;

    bool m_sl_initialised = false;
    UnityGfxRenderer m_api = UnityGfxRenderer::kUnityGfxRendererD3D12;
    void* m_Device = nullptr;

    uint32_t m_renderSizeX = 0;
    uint32_t m_renderSizeY = 0;
    uint32_t m_outputSizeX = 0;
    uint32_t m_outputSizeY = 0;

    bool m_dlss_available = false;
    sl::DLSSOptions m_dlss_consts{};

    bool m_dlssg_available = false;
    bool m_dlssg_triggerswapchainRecreation = false;
    bool m_dlssg_shoudLoad = false;
    sl::DLSSGOptions m_dlssg_consts{};
    sl::DLSSGState m_dlssg_settings{};

    bool m_reflex_available = false;
    sl::ReflexOptions m_reflex_consts{};
    bool m_reflex_driverFlashIndicatorEnable = false;
    bool m_pcl_available = false;

    uint32_t m_currentSimFrameID = 0;
    sl::FrameToken *m_currentSimFrame;
    uint32_t m_currentRenderFrameID = 0;
    sl::FrameToken *m_currentRenderFrame;
    sl::ViewportHandle m_viewport = {0};

    static sl::Resource allocateResourceCallback(const sl::ResourceAllocationDesc* resDesc, void* device);
    static void releaseResourceCallback(sl::Resource* resource, void* device);

    void SetDevice(void* pDevice);
    void UpdateFeatureAvailable(void* pDevice);

    sl::FrameToken* m_renderFrame = nullptr;
    sl::FrameToken* m_presentFrame = nullptr;
    sl::FrameToken* UpdateRenderFrame();
    sl::FrameToken* UpdatePresentFrame();

    struct
    {
        bool checkSig = true;
        bool enableLog = false;
        bool useNewSetTagAPI = true;
        bool allowSMSCG = false;
    } m_SLOptions{};

public:

    static SLWrapper& Get();
    SLWrapper(const SLWrapper&) = delete;
    SLWrapper(SLWrapper&&) = delete;
    SLWrapper& operator=(const SLWrapper&) = delete;
    SLWrapper& operator=(SLWrapper&&) = delete;

    virtual void SetSLOptions(const bool checkSig, const bool enableLog, const bool useNewSetTagAPI, const bool allowSMSCG);

    bool Initialize_preDevice();
    bool Initialize(UnityGfxRenderer api, void* pDevice);
    void Shutdown();
    void QueueGPUWaitOnSyncObjectSet(void* pDevice, void* cmdQType, void* syncObj, uint64_t syncObjVal);

    sl::FeatureRequirements GetFeatureRequirements(sl::Feature feature);
    sl::FeatureVersion GetFeatureVersion(sl::Feature feature);

    void SetViewportHandle(sl::ViewportHandle vpHandle)
    {
        m_viewport = vpHandle;
    }

    void GetSLResource(sl::Resource& slResource, void* inputTex);
    void SetSLConsts(const CameraData& cameraData);
    void FeatureLoad(sl::Feature feature, const bool turn_on);

    void TagResources_General(
        void* commandList,
        void* motionVectors,
        void* depth,
        void* finalColorHudless);

    void TagResources_DLSS_NIS(
        void* commandList,
        void* output,
        void* input);

    void TagResources_DLSS_FG(
        void* commandList,
        bool validViewportExtent = false,
        sl::Extent backBufferExtent = {});

    void SetDLSSOptions(const sl::DLSSOptions consts);
    bool GetDLSSAvailable() { return m_dlss_available; }
    bool GetDLSSLastEnable() { return m_dlss_consts.mode != sl::DLSSMode::eOff; }
    DLSSSettings QueryDLSSOptimalSettings(const sl::DLSSOptions& consts);
    void EvaluateDLSS(void* commandList);
    void CleanupDLSS(bool wfi);

    bool GetReflexAvailable() { return m_reflex_available; }
    bool GetPCLAvailable() const { return m_pcl_available; }
    void SetReflexConsts(const sl::ReflexOptions consts);
    void ReflexCallback_Sleep(uint32_t frameID);
    void ReflexCallback_SimStart(uint32_t frameID);
    void ReflexCallback_SimEnd(uint32_t frameID);
    void ReflexCallback_RenderStart(uint32_t frameID);
    void ReflexCallback_RenderEnd(uint32_t frameID);
    void ReflexCallback_PresentStart();
    void ReflexCallback_PresentEnd();

    void ReflexTriggerFlash();
    void ReflexTriggerPcPing();
    void QueryReflexStats(bool& reflex_lowLatencyAvailable, bool& reflex_flashAvailable, std::string& stats);
    void SetReflexFlashIndicator(bool enabled) { m_reflex_driverFlashIndicatorEnable = enabled; }
    bool GetReflexFlashIndicatorEnable() { return m_reflex_driverFlashIndicatorEnable; }

    void SetDLSSGOptions(const sl::DLSSGOptions consts);
    bool GetDLSSGAvailable() { return m_dlssg_available; }
    bool GetDLSSGLastEnable() { return m_dlssg_consts.mode != sl::DLSSGMode::eOff; }
    void Set_DLSSG_SwapChainRecreation(bool on)
    {
        m_dlssg_triggerswapchainRecreation = true;
        m_dlssg_shoudLoad = on;
    }
    void QueryDLSSGState(uint64_t& estimatedVRamUsage, int& fps_multiplier, sl::DLSSGStatus& status, int& minSize, int& framesMax, void*& pFence, uint64_t& fenceValue);
    bool Get_DLSSG_SwapChainRecreation(bool& turn_on) const;
    void Quiet_DLSSG_SwapChainRecreation() { m_dlssg_triggerswapchainRecreation = false; }
    void CleanupDLSSG(bool wfi);
    uint64_t GetDLSSGLastFenceValue();

    void SetReflexCameraData(sl::FrameToken& frameToken, const sl::ReflexCameraData& cameraData);

    sl::Result SetTag(const sl::ResourceTag* resources, uint32_t numResources, sl::CommandBuffer* cmdBuffer)
    {
        return m_SLOptions.useNewSetTagAPI ? slSetTagForFrame(*m_currentRenderFrame, m_viewport, resources, numResources, cmdBuffer) : slSetTag(m_viewport, resources, numResources, cmdBuffer);
    }
};

