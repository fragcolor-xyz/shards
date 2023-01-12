#pragma once
#ifndef A44AEEB5_7CD7_4AAC_A2DE_988863BD5160
#define A44AEEB5_7CD7_4AAC_A2DE_988863BD5160

#include "openxr-integration/OpenXRSystem.h"
#include "context.hpp"
#include "context_internal.hpp"
#include "context_direct.hpp"
#include "platform_surface.hpp"
#include "wgpu_handle.hpp"

#include "context_xr_gfx_data.hpp"

#include <Vulkan-Headers/include/vulkan/vulkan_handles.hpp>

#if defined(GFX_WINDOWS)
//#define VK_USE_PLATFORM_WIN32_KHR //[t] TODO: Guus, if I uncomment this, all of vulkan shits the bed with 359 errors. Why?
#endif



#include <Vulkan-Headers/include/vulkan/vulkan.hpp>
#include <Vulkan-Headers/include/vulkan/vulkan_raii.hpp>

#include <gfx_rust.h>

#include "spdlog/spdlog.h"

#include "context_direct.hpp"

//#include "openxr-integration/Headset.h"

#define LOG_T

namespace gfx {
  
  //[t] This really confused me because it's kinda only made for mirror views
  //[t] but it needs t be general purpose / shared between headset and mirror view,
  //[t] but it's different from the boilerplates I studied,
  //[t] and it also uses 2 graphics apis at the same time. :)
  //[t] and doesn't explain what it sets up and links and the point of what it's doing.... X_X
  //[t] So:
  //[t] NOTE: Prototype OpenXR binding code. Just for vulkan and wgpu binding, so the gfx side only. 
  //[t] Does not touch openXR here but needs to be integrated into the xr system. 
  //[t] Needed to create vk and wgpu devices and swapchains etc. The vk ones need to be used by both xr Headset.cpp AND ALSO the Mirror View.
  // The openxr vulkan extensions requires the following:
  // - Specific extensions on instance creation
  // - Specific physical device as returned by OpenXR
  // - Specific extensions on device creation
  // - Need to retrieve the created instance, device handles and queue indices
  struct ContextXrGfxBackend : public IContextBackend { 
    WGPUSurface wgpuSurface{};

    std::shared_ptr<WGPUVulkanShared> wgpuVulkanShared = std::make_shared<WGPUVulkanShared>();

    //[T] Called by context.cpp, to create a mirrorview, and called by my context_xr to create headset.
    //[t] Some comments would have REALLY helped here explaining a bit what the ideas are.
    WGPUInstance createInstance() override 
    { 
      #ifdef LOG_T
      spdlog::info("[log][t] IContextBackend::ContextXrGfxBackend::createInstance().");
      #endif
      //[t] xr system 
      {
        OpenXRSystem& openXRSystem = OpenXRSystem::getInstance();
        //std::shared_ptr<WGPUVulkanShared> wgpuVulkanShared = std::make_shared<WGPUVulkanShared>();
        if(openXRSystem.InitOpenXR(true, OpenXRSystem::defaultHeadset) == 1){
          spdlog::error("[log][t] IContextBackend::ContextXrGfxBackend::createInstance: error at openXRSystem.InitOpenXR(.");
          return nullptr;
        }
      }
      
      
      
      // Create gpu instance
      WGPUInstanceDescriptor desc{}; 
      WGPUInstanceDescriptorVK descVk{};
      if (getDefaultWgpuBackendType() == WGPUBackendType_Vulkan) {
        std::vector<const char *> requiredExtensions = {};

        descVk = {
            .chain{.sType = (WGPUSType)WGPUNativeSTypeEx_InstanceDescriptorVK},
            .requiredExtensions = requiredExtensions.data(),
            .requiredExtensionCount = (uint32_t)requiredExtensions.size(),
        };
        desc.nextInChain = &descVk.chain;
      }

      wgpuVulkanShared->wgpuInstance = wgpuCreateInstanceEx(&desc);
      if (!wgpuVulkanShared->wgpuInstance) {
        spdlog::error("[log][t] IContextBackend::ContextXrGfxBackend::createInstance: Failed to create WGPUInstance.");
        throw std::runtime_error("Failed to create WGPUInstance");
      }

      WGPUInstanceProperties props{};
      WGPUInstancePropertiesVK propsVk{
          .chain = {.sType = WGPUSType(WGPUNativeSTypeEx_InstancePropertiesVK)},
      };
      if (getDefaultWgpuBackendType() == WGPUBackendType_Vulkan) {
        props.nextInChain = &propsVk.chain;
      }
      wgpuInstanceGetPropertiesEx(wgpuVulkanShared->wgpuInstance, &props); 

      wgpuVulkanShared->vkLoader = vk::DispatchLoaderDynamic(PFN_vkGetInstanceProcAddr(propsVk.getInstanceProcAddr));

      wgpuVulkanShared->vkInstance = vk::Instance(VkInstance(propsVk.instance));
      wgpuVulkanShared->vkLoader.init(wgpuVulkanShared->vkInstance);

      
      //auto vkDevices = wgpuVulkanShared->vkInstance.enumeratePhysicalDevices(wgpuVulkanShared->vkLoader);
      //for (auto &vkDevice : vkDevices) {
      //  auto properties = vkDevice.getProperties(wgpuVulkanShared->vkLoader);
      //  auto name = std::string(properties.deviceName.begin(), properties.deviceName.end());
      //  SPDLOG_INFO("vulkan physical device: {}", name);
      //  wgpuVulkanShared->vkPhysicalDevice = vkDevice;
      //  break;
      //}
      //assert(wgpuVulkanShared->vkPhysicalDevice);
      
      
      //[t] xr system 
      {
        OpenXRSystem& openXRSystem = OpenXRSystem::getInstance();
        openXRSystem.SetWgpuVulkanShared(wgpuVulkanShared);
        //[t] TODO: print some message to the user to say it's not connected.
        //[t] (If a headset is sleeping but present, it works successfully)
        if(!openXRSystem.checkXRDeviceReady(OpenXRSystem::defaultHeadset)){
          spdlog::error("[log][t] IContextBackend::ContextXrGfxBackend::createInstance: error at openXRSystem.checkXRDeviceReady(.");
          return nullptr;
        }
        
        openXRSystem.GetVulkanExtensionsFromOpenXRInstance();
      }

      #ifdef LOG_T
      spdlog::info("[log][t] IContextBackend::ContextXrGfxBackend::createInstance: End.");
      #endif
      return wgpuVulkanShared->wgpuInstance;
    }

    WGPUSurface createSurface(Window &window, void *overrideNativeWindowHandle) override
    {
      #ifdef LOG_T
      spdlog::info("[log][t] IContextBackend::ContextXrGfxBackend::createSurface().");
      #endif
      void *surfaceHandle = overrideNativeWindowHandle;

    #if GFX_APPLE
      if (!surfaceHandle) {
        metalViewContainer = std::make_unique<MetalViewContainer>(window->window);
        surfaceHandle = metalViewContainer->layer;
      }
    #endif

      WGPUPlatformSurfaceDescriptor surfDesc(window.window, surfaceHandle);
      wgpuSurface = wgpuInstanceCreateSurface(wgpuVulkanShared->wgpuInstance, &surfDesc);

      return wgpuSurface;
    }

    std::shared_ptr<AdapterRequest> requestAdapter() override {
      //[t] xr system 
      {
        OpenXRSystem& openXRSystem = OpenXRSystem::getInstance();
        if(!openXRSystem.CreatePhysicalDevice()){
          spdlog::error("[log][t] IContextBackend::ContextXrGfxBackend::createInstance: error at openXRSystem->CreatePhysicalDevice()");
          return nullptr;
        }
      }

      WGPURequestAdapterOptionsVK optionsVk{
          .chain = {.sType = WGPUSType(WGPUNativeSTypeEx_RequestAdapterOptionsVK)},
          .physicalDevice = wgpuVulkanShared->vkPhysicalDevice,
      };

      WGPURequestAdapterOptions options{};
      options.powerPreference = WGPUPowerPreference_HighPerformance;
      options.compatibleSurface = wgpuSurface;
      options.forceFallbackAdapter = false;
      options.nextInChain = &optionsVk.chain;

      auto cb = [&](WGPURequestAdapterStatus status, WGPUAdapter adapter, const char *msg) { wgpuVulkanShared->wgpuAdapter = adapter; };
      return AdapterRequest::create(wgpuVulkanShared->wgpuInstance, options, AdapterRequest::Callbacks{cb});
    }

    void deviceCreated(WGPUDevice inDevice) {
      #ifdef LOG_T
      spdlog::info("[log][t] IContextBackend::ContextXrGfxBackend::deviceCreated().");
      #endif
      wgpuVulkanShared->wgpuDevice = inDevice;
      wgpuVulkanShared->wgpuQueue = wgpuDeviceGetQueue(wgpuVulkanShared->wgpuDevice);




      //---------------
      // probably required physical device featurs for xr
      //added in adapter.rs: 
      //.shader_storage_image_multisample(
      //              true,
      //          )
      //---------------
      
      WGPUDevicePropertiesVK propsVk{ 
          .chain = {.sType = WGPUSType(WGPUNativeSTypeEx_DevicePropertiesVK)},
      };
      WGPUDeviceProperties props{.nextInChain = &propsVk.chain};
      wgpuDeviceGetPropertiesEx(wgpuVulkanShared->wgpuDevice, &props);

      wgpuVulkanShared->vkDevice = vk::Device(VkDevice(propsVk.device)); 
      wgpuVulkanShared->vkQueue = vk::Queue(VkQueue(propsVk.queue));
      wgpuVulkanShared->queueIndex = propsVk.queueIndex;
      wgpuVulkanShared->queueFamilyIndex = propsVk.queueFamilyIndex;

      #ifdef LOG_T
      spdlog::info("[log][t] IContextBackend::ContextXrGfxBackend::deviceCreated: VkDevice, vkQueue created. End.");
      #endif
    }

    std::shared_ptr<DeviceRequest> requestDevice() override {
      #ifdef LOG_T
      spdlog::info("[log][t] IContextBackend::ContextXrGfxBackend::requestDevice().");
      #endif
      std::vector<const char *> requiredExtensions = {};

      //[t] xr system 
      OpenXRSystem& openXRSystem = OpenXRSystem::getInstance();
      {
        requiredExtensions = openXRSystem.GetVulkanXrExtensions();
      }

      WGPUDeviceDescriptorVK deviceDescVk{
          .chain = {.sType = WGPUSType(WGPUNativeSTypeEx_DeviceDescriptorVK)},
          .requiredExtensions = requiredExtensions.data(),
          .requiredExtensionCount = (uint32_t)requiredExtensions.size(),
      };

      WGPUDeviceDescriptor deviceDesc = {};
      deviceDesc.nextInChain = &deviceDescVk.chain;

      WGPURequiredLimits requiredLimits = {.limits = wgpuGetDefaultLimits()};
      deviceDesc.requiredLimits = &requiredLimits;

      auto cb = [&](WGPURequestDeviceStatus status, WGPUDevice device, const char *msg) { 
        //[t] xr system 
        { 
          openXRSystem.CheckXrGraphicsRequirementsVulkanKHR();
        }
        deviceCreated(device);  
      };
      
      std::shared_ptr<DeviceRequest> devReq = DeviceRequest::create(wgpuVulkanShared->wgpuAdapter, deviceDesc, DeviceRequest::Callbacks{cb});
      #ifdef LOG_T
      spdlog::info("[log][t] IContextBackend::ContextXrGfxBackend::requestDevice() returning DeviceRequest.");
      #endif
      

      return devReq;
    }
    
    //std::shared_ptr<WGPUVulkanShared> getWgpuVulkanShared() const {
    //  
    //  return wgpuVulkanShared; 
    //}

    //[t] TODO: figure out how to modify this to create not only the OpenXRMirrorView but also (link to) the headset views?
    //[t] NODE: this is tied to createSurface( as well
    std::vector<std::shared_ptr<IContextMainOutput>> createMainOutput(Window &window) override { 
      #ifdef LOG_T
      spdlog::info("[log][t] IContextBackend::ContextXrGfxBackend::createMainOutput(): Creating Headset and mirrorView.");
      #endif
      // return std::make_shared<ContextWindowOutput>(wgpuInstance, wgpuAdapter, wgpuDevice, wgpuSurface, window);
      // return std::make_shared<OpenXRMirrorView>(wgpuInstance, wgpuAdapter, wgpuDevice, window);
      //return std::make_shared<OpenXRMirrorView>(wgpuVulkanShared.wgpuInstance, wgpuVulkanShared.wgpuAdapter, wgpuVulkanShared.wgpuDevice, window);
      //return std::make_shared<OpenXRMirrorView>(wgpuVulkanShared, window);
      
      //TODO: headset with both eyes
      //TODO: also mirrorview
      OpenXRSystem& openXRSystem = OpenXRSystem::getInstance();
      std::vector<std::shared_ptr<IContextMainOutput>> mainOutputs = 
      { 
        openXRSystem.createHeadset(wgpuVulkanShared, true)
        //[t] UNCOMMENT FOR MIRROR VIEW / Window:
        //,std::make_shared<ContextWindowOutput>(
        //                                      wgpuVulkanShared->wgpuInstance, 
        //                                      wgpuVulkanShared->wgpuAdapter, 
        //                                      wgpuVulkanShared->wgpuDevice, 
        //                                      wgpuSurface, 
        //                                      window)
      };
      #ifdef LOG_T
      spdlog::info("[log][t] IContextBackend::ContextXrGfxBackend::createMainOutput: returning mainOutputs: Headset and mirrorView: {}.",mainOutputs.size());
      #endif
      return mainOutputs;
    } 

  };
} // namespace gfx

#endif /* A44AEEB5_7CD7_4AAC_A2DE_988863BD5160 */
