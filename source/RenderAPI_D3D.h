#pragma once

#include <sl.h>
#include <sl_consts.h>
#include <sl_hooks.h>
#include <sl_version.h>
#include <dxgi.h>

#include "RenderAPI.h"

class RenderAPI_D3D : public RenderAPI
{
protected:
    DLSSMode m_dlss_mode = DLSSMode::eOff;
    void UpscaleTextureDLSS(void* context) const;

public:
    ~RenderAPI_D3D() {};

    virtual bool SupportDLSS() override;
    virtual void CleanupDLSS() override;
    virtual DLSSSettings QueryDLSSOptimalSettings(int outputSizeX, int outputSizeY, DLSSMode mode) override;
    virtual void SetDLSSOptions(DLSSMode mode) override { m_dlss_mode = mode; }
    virtual void ReflexCallback_Sleep(uint32_t frameID) override;
	virtual void ReflexCallback_SimStart(uint32_t frameID) override;
    virtual void ReflexCallback_SimEnd(uint32_t frameID) override;
    virtual void ReflexCallback_RenderStart(uint32_t frameID) override;
    virtual void ReflexCallback_RenderEnd(uint32_t frameID) override;

    virtual void* GetDevice() = 0;
    virtual LUID GetAdapterLuid() = 0;
    virtual sl::Resource AllocateBuffer(const sl::ResourceAllocationDesc* resDesc, void* device) = 0;
    virtual sl::Resource AllocateTexture(const sl::ResourceAllocationDesc* resDesc, void* device) = 0;
};
