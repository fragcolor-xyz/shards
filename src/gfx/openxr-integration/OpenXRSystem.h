#pragma once
//#include "context.hpp"
//#include "context_xr_gfx.hpp"
#include "context_xr.h"
#include "Headset.h"


class OpenXRSystem {
  public: 
    // [t] it's instantiated a single time, and no need to check for null pointers
    // [t] BECAUSE OF MAGIC(STATICS): https://blog.mbedded.ninja/programming/languages/c-plus-plus/magic-statics/
    // [t] it should also be thread safe
    // https://stackoverflow.com/questions/1008019/c-singleton-design-pattern
    static OpenXRSystem& getInstance()
    {
        static OpenXRSystem instance; // Guaranteed to be destroyed.
                                      // Instantiated on first use.
        return instance;
    }
    struct HeadsetType{
        XrViewConfigurationType viewType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
        XrEnvironmentBlendMode environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
        XrFormFactor xrFormFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
    };

  private: 
    bool isMultipass;
    //static OpenXRSystem instance;
    OpenXRSystem(){};

    std::shared_ptr<Context_XR> context_xr = nullptr;
    
    std::shared_ptr<Headset> headset = nullptr;
    
    //OpenXRSystem(OpenXRSystem const&);// Don't Implement
    //void operator=(OpenXRSystem const&);// Don't implement
    
  public:    
    static const HeadsetType defaultHeadset; 
    
    OpenXRSystem(OpenXRSystem const&) = delete;
    void operator=(OpenXRSystem const&) = delete;

    bool checkXRDeviceReady(HeadsetType heasetType);

    int InitOpenXR(std::shared_ptr<gfx::WGPUVulkanShared> wgpuUVulkanShared, HeadsetType headsetType);

    std::shared_ptr<gfx::IContextMainOutput> createHeadset(std::shared_ptr<gfx::WGPUVulkanShared> wgpuUVulkanShared, bool isMultipass);
    int somethingSomethingMakeFrames();

    bool getIsMultipass();

    ~OpenXRSystem();
};