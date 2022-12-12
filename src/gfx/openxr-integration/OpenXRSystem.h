#pragma once
//#include "context.hpp"
//#include "context_xr_gfx.hpp"
#include "context_xr.h"
#include "Headset.h"


class OpenXRSystem {
  public: 
    struct HeadsetType{
        XrViewConfigurationType viewType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
        XrEnvironmentBlendMode environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
        XrFormFactor xrFormFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
    };

  private: 
    bool isMultipass;
    static OpenXRSystem xrs;
    OpenXRSystem(){};

    std::shared_ptr<gfx::WGPUVulkanShared> wgpuUVulkanShared;
    Context_XR* context_xr = nullptr;
    
    std::shared_ptr<Headset> headset = nullptr;
    
    
  public:    
    static const HeadsetType defaultHeadset; 
    static OpenXRSystem& getInstance();

    bool checkXRDeviceReady(HeadsetType heasetType);

    int InitOpenXR(std::shared_ptr<gfx::WGPUVulkanShared> wgpuUVulkanShared, HeadsetType headsetType);

    std::shared_ptr<gfx::IContextMainOutput> createHeadset(bool isMultipass);
    int somethingSomethingMakeFrames();

    bool getIsMultipass();

    ~OpenXRSystem();
};