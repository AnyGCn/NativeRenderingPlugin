#pragma once

#include "RenderAPI.h"

class RenderAPI_D3D : public RenderAPI
{
protected:
    DLSSMode m_dlss_mode = DLSSMode::eOff;
    void UpscaleTextureDLSS(void *context) const;

public:
    ~RenderAPI_D3D() {};

    virtual bool SupportDLSS() override;
    virtual void CleanupDLSS() override;
    virtual DLSSSettings QueryDLSSOptimalSettings(DLSSMode mode) override;
    virtual void SetDLSSOptions(DLSSMode mode) override { m_dlss_mode = mode; }
    virtual void ReflexCallback_Sleep(uint32_t frameID) override;
	virtual void ReflexCallback_SimStart(uint32_t frameID) override;
    virtual void ReflexCallback_SimEnd(uint32_t frameID) override;
    virtual void ReflexCallback_RenderStart(uint32_t frameID) override;
    virtual void ReflexCallback_RenderEnd(uint32_t frameID) override;

    virtual void SetCameraData(void* data) override;
};
