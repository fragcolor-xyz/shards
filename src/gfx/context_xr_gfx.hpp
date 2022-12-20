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


/*
#if GFX_WINDOWS
#define VK_USE_PLATFORM_WIN32_KHR //[t] TODO: Guus, if I uncomment this, all of vulkan shits the bed with 359 errors. Why?
#endif
*/


#include <Vulkan-Headers/include/vulkan/vulkan.hpp>
#include <Vulkan-Headers/include/vulkan/vulkan_raii.hpp>

#include <gfx_rust.h>

#include "spdlog/spdlog.h"

//#include "openxr-integration/Headset.h"

namespace gfx {
  


  //[t] TODO: should move this out of this file. This struct is JUST for Mirror View. 
  // [t] This file should be for gfx vk/wgpu context backend shared by the xr headset and also mirror view.
  // [t] Although the xr headset itself also needs swapchains and view textures, so this was very confusing because it's similar but also very different from the xr boilerplates
  struct OpenXRMirrorView:public IContextMainOutput
  {
    Window &window;

    std::shared_ptr<WGPUVulkanShared> wgpuVulkanShared;//[t] created by ContextXrGfxBackend, passed as param to the construtor here

    // Temporary mirror surface on the window
    vk::SurfaceKHR mirrorSurface;
    vk::SwapchainKHR mirrorSwapchain;
    std::vector<vk::Image> mirrorSwapchainImages;
    std::vector<WgpuHandle<WGPUTextureView>> mirrorTextureViews;
    
    vk::Fence fence;

    uint32_t currentImageIndex{};
    int2 currentSize;
    WGPUTextureFormat currentFormat = WGPUTextureFormat_Undefined;
    WGPUTextureView currentView{};  

    uint32_t frame = 0;
    uint32_t debugs = 3;             

    OpenXRMirrorView(std::shared_ptr<WGPUVulkanShared> &wgpuVulkanShared, Window &window)
        : window(window), wgpuVulkanShared(wgpuVulkanShared) {
      spdlog::info("[log][t] IContextMainOutput::ContextXrGfxBackend::OpenXRMirrorView(std::shared_ptr<WGPUVulkanShared> &wgpuVulkanShared, Window &window).");
      createMirrorSurface(); 
      createMirrorSwapchain();

      fence = wgpuVulkanShared->vkDevice.createFence(vk::FenceCreateInfo(), nullptr, wgpuVulkanShared->vkLoader);
      spdlog::info("[log][t] IContextMainOutput::ContextXrGfxBackend::OpenXRMirrorView: End.");
    }

    void createMirrorSwapchain(VkSwapchainKHR oldSwapchain = nullptr) {
      spdlog::info("[log][t] IContextMainOutput::ContextXrGfxBackend::createMirrorSwapchain(VkSwapchainKHR oldSwapchain = nullptr).");
      assert(mirrorSurface);

      auto &vkLoader = wgpuVulkanShared->vkLoader;

      currentSize = window.getDrawableSize();

      auto surfaceFormats = wgpuVulkanShared->vkPhysicalDevice.getSurfaceFormatsKHR(mirrorSurface, vkLoader);
      std::optional<vk::SurfaceFormatKHR> selectedFormatOption;
      for (auto &format : surfaceFormats) {
        if (format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
          switch (format.format) {
          case vk::Format::eB8G8R8A8Srgb:
            currentFormat = WGPUTextureFormat_BGRA8UnormSrgb;
            selectedFormatOption = format;
            break;
          default:
            continue;
          }
        }
        if (selectedFormatOption)
          break;
      }

      if (!selectedFormatOption){
        spdlog::error("[log][t] IContextMainOutput::ContextXrGfxBackend::createMirrorSwapchain: error at No supported surface formats found.");
        throw std::runtime_error("No supported surface formats found");
      }
      auto selectedFormat = selectedFormatOption.value();

      // Check src\gfx\rust\wgpu\wgpu-hal\src\vulkan\device.rs for reference
      vk::SwapchainCreateInfoKHR scInfo;
      scInfo.setSurface(mirrorSurface);
      scInfo.setMinImageCount(3);
      scInfo.setImageFormat(selectedFormat.format);
      scInfo.setImageColorSpace(selectedFormat.colorSpace);
      scInfo.setImageExtent(vk::Extent2D(currentSize.x, currentSize.y));
      scInfo.setImageArrayLayers(1);
      scInfo.setImageUsage(vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferDst);
      scInfo.setImageSharingMode(vk::SharingMode::eExclusive);
      scInfo.setPreTransform(vk::SurfaceTransformFlagBitsKHR::eIdentity);
      scInfo.setCompositeAlpha(vk::CompositeAlphaFlagBitsKHR::eOpaque);
      scInfo.setPresentMode(vk::PresentModeKHR::eImmediate);
      scInfo.setClipped(true);
      scInfo.setOldSwapchain(oldSwapchain);
      // [t] ReCreate Swapchain
      mirrorSwapchain = wgpuVulkanShared->vkDevice.createSwapchainKHR(scInfo, nullptr, vkLoader);

      if (oldSwapchain)
        wgpuVulkanShared->vkDevice.destroySwapchainKHR(oldSwapchain, nullptr, vkLoader);

      if (!mirrorSwapchain){
        spdlog::error("[log][t] IContextMainOutput::ContextXrGfxBackend::createMirrorSwapchain: error at Failed to create swapchain.");
        throw std::runtime_error("Failed to create swapchain");
      }

      // [t] Retrieve the new swapchain images
      mirrorSwapchainImages = wgpuVulkanShared->vkDevice.getSwapchainImagesKHR(mirrorSwapchain, vkLoader); 

      mirrorTextureViews.clear();
      for (auto &swapchainImage : mirrorSwapchainImages) {
        WGPUTextureDescriptor textureDesc{
            .usage = WGPUTextureUsage::WGPUTextureUsage_CopyDst | WGPUTextureUsage::WGPUTextureUsage_RenderAttachment,
            .dimension = WGPUTextureDimension_2D,
            .size = WGPUExtent3D{.width = uint32_t(currentSize.x), .height = uint32_t(currentSize.y), .depthOrArrayLayers = 1},
            .format = currentFormat,
            .mipLevelCount = 1,
            .sampleCount = 1,
            .viewFormatCount = 0,
        };

        WGPUTextureViewDescriptor viewDesc{
            .format = textureDesc.format,
            .dimension = WGPUTextureViewDimension_2D,
            .baseMipLevel = 0,
            .mipLevelCount = 1,
            .baseArrayLayer = 0,
            .arrayLayerCount = 1,
            .aspect = WGPUTextureAspect_All,
        };

        WGPUExternalTextureDescriptorVK extDescVk{
            .chain = {.sType = WGPUSType(WGPUNativeSTypeEx_ExternalTextureDescriptorVK)},
            .image = swapchainImage,
        };
        WGPUExternalTextureDescriptor extDesc{
            .nextInChain = &extDescVk.chain,
        }; 
    
        WGPUTextureView textureView = wgpuCreateExternalTextureView(wgpuVulkanShared->wgpuDevice, &textureDesc, &viewDesc, &extDesc);
        assert(textureView);  
  
        mirrorTextureViews.emplace_back(toWgpuHandle(textureView));
      }
      spdlog::info("[log][t] IContextMainOutput::ContextXrGfxBackend::createMirrorSwapchain: End.");
    }


    void createMirrorSurface() {
      spdlog::info("[log][t] IContextMainOutput::ContextXrGfxBackend::createMirrorSurface()...");
      #if VK_USE_PLATFORM_WIN32_KHR //[t] NOTE: I replaced "GFX_WINDOWS" with VK_USE_PLATFORM_WIN32_KHR, and disabled the GFX_WINDOWS define from up top because it was giving an error. 
      vk::Win32SurfaceCreateInfoKHR surfInfo;
      surfInfo.setHwnd(HWND(window.getNativeWindowHandle()));
      mirrorSurface = wgpuVulkanShared->instance.createWin32SurfaceKHR(surfInfo, nullptr, wgpuVulkanShared->loader);
      if (!mirrorSurface)
        spdlog::error("[log][t] IContextMainOutput::ContextXrGfxBackend::createMirrorSurface: error at mirrorSurface = wgpuVulkanShared->instance.createWin32SurfaceKHR(surfInfo, nullptr, wgpuVulkanShared->loader);");
        throw std::runtime_error("Failed to create surface");
      #else
      static_assert("Platform surface not implemented");
      #endif

      auto result =
          wgpuVulkanShared->vkPhysicalDevice.getSurfaceSupportKHR(wgpuVulkanShared->queueFamilyIndex, mirrorSurface, wgpuVulkanShared->vkLoader);
      if (!result){
        spdlog::error("[log][t] IContextMainOutput::ContextXrGfxBackend::createMirrorSurface: error at Unsupported surface.");
        throw std::runtime_error("Unsupported surface");
      }
      spdlog::info("[log][t] IContextMainOutput::ContextXrGfxBackend::createMirrorSurface: End.");
    }

    const int2 &getSize() const override { return currentSize; }

    WGPUTextureFormat getFormat() const override { return currentFormat; }

    std::vector<WGPUTextureView> requestFrame() override {
      if(frame<debugs){
        frame++;
        spdlog::info("[log][t] IContextMainOutput::ContextXrGfxBackend::requestFrame().");
      }
      assert(!currentView); 
      std::vector<WGPUTextureView> views;
      vk::ResultValue<uint32_t> result =
          wgpuVulkanShared->vkDevice.acquireNextImageKHR(mirrorSwapchain, UINT64_MAX, {}, fence, wgpuVulkanShared->vkLoader);
      if (result.result == vk::Result::eSuccess) {
        vk::Result waitResult = wgpuVulkanShared->vkDevice.waitForFences(fence, true, UINT64_MAX, wgpuVulkanShared->vkLoader);
        assert(waitResult == vk::Result::eSuccess);

        wgpuVulkanShared->vkDevice.resetFences(fence, wgpuVulkanShared->vkLoader);

        currentImageIndex = result.value;  
        currentView = mirrorTextureViews[currentImageIndex];
        views = {currentView};
        if(frame<debugs){
        
          spdlog::info("[log][t] IContextMainOutput::ContextXrGfxBackend::requestFrame: End.");
        }
        return views;
      } else {
        if(frame<debugs){
         
          spdlog::info("[log][t] IContextMainOutput::ContextXrGfxBackend::requestFrame: End.");
        }
        return views;
      }
    }

    std::vector<IContextCurrentFramePayload> getCurrentFrame() const override {
      /*
      if(frame<debugs){
        
        spdlog::info("[log][t] IContextMainOutput::ContextXrGfxBackend::getCurrentFrame().");
      }*/
      assert(currentView);
      std::vector<gfx::IContextCurrentFramePayload> payload;
      size_t mirrorViews = 1;
      payload.resize(mirrorViews);
      for(size_t mv = 0; mv<mirrorViews; mv++){
        payload.at(mv).wgpuTextureView = currentView;
      }
      //std::vector<IContextCurrentFramePayload> views = {currentView};
      /*
      if(frame<debugs){
        
        spdlog::info("[log][t] IContextMainOutput::ContextXrGfxBackend::getCurrentFrame; End.");
      }*/
      return payload;
    }


    void present() override {
      if(frame<debugs){
        
        spdlog::info("[log][t] IContextMainOutput::ContextXrGfxBackend::present().");
      }
      WGPUExternalPresentVK wgpuPresent{};
      wgpuPrepareExternalPresentVK(wgpuVulkanShared->wgpuQueue, &wgpuPresent);

      vk::Semaphore waitSemaphore = wgpuPresent.waitSemaphore ? VkSemaphore(wgpuPresent.waitSemaphore) : nullptr;

      vk::PresentInfoKHR presentInfo;
      presentInfo.setSwapchainCount(1);
      presentInfo.setPSwapchains(&mirrorSwapchain);
      presentInfo.setPImageIndices(&currentImageIndex);
      presentInfo.setPWaitSemaphores(&waitSemaphore);
      presentInfo.setWaitSemaphoreCount(1);

      // presentInfo.setWaitSemaphores(const vk::ArrayProxyNoTemporaries<const vk::Semaphore> &waitSemaphores_)
      vk::Result result = wgpuVulkanShared->vkQueue.presentKHR(&presentInfo, wgpuVulkanShared->vkLoader);
      assert(result == vk::Result::eSuccess);

      // Increment image
      currentView = nullptr;
      if(frame<debugs){
        
        spdlog::info("[log][t] IContextMainOutput::ContextXrGfxBackend::present: end.");
      }
    }
  };






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
      spdlog::info("[log][t] IContextBackend::ContextXrGfxBackend::createInstance().");
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

      /*
      auto vkDevices = wgpuVulkanShared->vkInstance.enumeratePhysicalDevices(wgpuVulkanShared->vkLoader);
      for (auto &vkDevice : vkDevices) {
        auto properties = vkDevice.getProperties(wgpuVulkanShared->vkLoader);
        auto name = std::string(properties.deviceName.begin(), properties.deviceName.end());
        SPDLOG_INFO("vulkan physical device: {}", name);
        wgpuVulkanShared->vkPhysicalDevice = vkDevice;
        break;
      }
      assert(wgpuVulkanShared->vkPhysicalDevice);
      */
      
      //[t] xr system 
      //[t] this should be created first/early, within the IContextBackend context_xr_gfx. 
      //[t] But we're using this weird wgpuVulkanShared loader thingy that requires to be set up before calling any vulkan functions from the xr code... 
      //[t] And also we have weird stuff here with deviceCreated(device)'s queue & wgpuDeviceGetPropertiesEx 
      {
        OpenXRSystem& openXRSystem = OpenXRSystem::getInstance();
        //std::shared_ptr<WGPUVulkanShared> wgpuVulkanShared = std::make_shared<WGPUVulkanShared>();
        if(openXRSystem.InitOpenXR(wgpuVulkanShared, true, OpenXRSystem::defaultHeadset) == 1){
          spdlog::error("[log][t] IContextBackend::ContextXrGfxBackend::createInstance: error at openXRSystem.InitOpenXR(.");
          return nullptr;
        }

        //[t] TODO: maybe have a loop here to wait / ask user to plug it in? Or just say it's not connected.
        //[t] (If a headset is sleeping but present, it works successfully)
        if(!openXRSystem.checkXRDeviceReady(OpenXRSystem::defaultHeadset)){
          spdlog::error("[log][t] IContextBackend::ContextXrGfxBackend::createInstance: error at openXRSystem.checkXRDeviceReady(.");
          return nullptr;
        }
        
        openXRSystem.GetVulkanExtensionsFromOpenXRInstance();

        
      }

      spdlog::info("[log][t] IContextBackend::ContextXrGfxBackend::createInstance: End.");
      return wgpuVulkanShared->wgpuInstance;
    }

    WGPUSurface createSurface(Window &window, void *overrideNativeWindowHandle) override
    {
      spdlog::info("[log][t] IContextBackend::ContextXrGfxBackend::createSurface().");
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
      spdlog::info("[log][t] IContextBackend::ContextXrGfxBackend::deviceCreated().");
      wgpuVulkanShared->wgpuDevice = inDevice;
      wgpuVulkanShared->wgpuQueue = wgpuDeviceGetQueue(wgpuVulkanShared->wgpuDevice);

      WGPUDevicePropertiesVK propsVk{
          .chain = {.sType = WGPUSType(WGPUNativeSTypeEx_DevicePropertiesVK)},
      };
      WGPUDeviceProperties props{.nextInChain = &propsVk.chain};
      wgpuDeviceGetPropertiesEx(wgpuVulkanShared->wgpuDevice, &props);

      wgpuVulkanShared->vkDevice = vk::Device(VkDevice(propsVk.device)); 
      wgpuVulkanShared->vkQueue = vk::Queue(VkQueue(propsVk.queue));
      wgpuVulkanShared->queueIndex = propsVk.queueIndex;
      wgpuVulkanShared->queueFamilyIndex = propsVk.queueFamilyIndex;

      spdlog::info("[log][t] IContextBackend::ContextXrGfxBackend::deviceCreated: VkDevice, vkQueue created. End.");
    }

    std::shared_ptr<DeviceRequest> requestDevice() override {
      spdlog::info("[log][t] IContextBackend::ContextXrGfxBackend::requestDevice().");
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
      spdlog::info("[log][t] IContextBackend::ContextXrGfxBackend::requestDevice() returning DeviceRequest.");

      

      return devReq;
    }
    /*
    std::shared_ptr<WGPUVulkanShared> getWgpuVulkanShared() const {
      
      return wgpuVulkanShared; 
    }*/

    //[t] TODO: figure out how to modify this to create not only the OpenXRMirrorView but also (link to) the headset views?
    //[t] NODE: this is tied to createSurface( as well
    std::vector<std::shared_ptr<IContextMainOutput>> createMainOutput(Window &window) override { 
      spdlog::info("[log][t] IContextBackend::ContextXrGfxBackend::createMainOutput(): Creating Headset and mirrorView.");
      // return std::make_shared<ContextWindowOutput>(wgpuInstance, wgpuAdapter, wgpuDevice, wgpuSurface, window);
      // return std::make_shared<OpenXRMirrorView>(wgpuInstance, wgpuAdapter, wgpuDevice, window);
      //return std::make_shared<OpenXRMirrorView>(wgpuVulkanShared.wgpuInstance, wgpuVulkanShared.wgpuAdapter, wgpuVulkanShared.wgpuDevice, window);
      //return std::make_shared<OpenXRMirrorView>(wgpuVulkanShared, window);
      
      //TODO: headset with both eyes
      //TODO: also mirrorview
      OpenXRSystem& openXRSystem = OpenXRSystem::getInstance();
      std::vector<std::shared_ptr<IContextMainOutput>> mainOutputs = 
      { 
        openXRSystem.createHeadset(wgpuVulkanShared, true), 
        std::make_shared<OpenXRMirrorView>(wgpuVulkanShared, window) 
      };
      spdlog::info("[log][t] IContextBackend::ContextXrGfxBackend::createMainOutput: returning mainOutputs: Headset and mirrorView.");
      return mainOutputs;
    } 

  };
} // namespace gfx

#endif /* A44AEEB5_7CD7_4AAC_A2DE_988863BD5160 */
