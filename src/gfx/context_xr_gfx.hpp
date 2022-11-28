#ifndef A44AEEB5_7CD7_4AAC_A2DE_988863BD5160
#define A44AEEB5_7CD7_4AAC_A2DE_988863BD5160

#include "context_internal.hpp"
#include "context_direct.hpp"
#include "platform_surface.hpp"
#include "wgpu_handle.hpp"

#if GFX_WINDOWS
#define VK_USE_PLATFORM_WIN32_KHR
#endif

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

#include <gfx_rust.h>

#include "Headset.h"

namespace gfx {

struct WGPUVulkanShared {
  vk::DispatchLoaderDynamic loader;
  vk::PhysicalDevice physicalDevice;//[t] VkPhysicalDevice
  vk::Instance instance;//[t] VkInstance
  vk::Device device;//[t] VkDevice
  vk::Queue queue;//[t] VkQueue, draw queue. The present queue is wgpuPresent afaiks

  uint32_t queueIndex;
  uint32_t queueFamilyIndex;

  WGPUInstance wgpuInstance{};
  WGPUAdapter wgpuAdapter{};
  WGPUDevice wgpuDevice{};
  WGPUQueue wgpuQueue{};
};

struct VulkanOpenXRSwapchain : public IContextMainOutput 
{
  Window &window;

  std::shared_ptr<WGPUVulkanShared> wgpuVulkanShared;

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

  VulkanOpenXRSwapchain(std::shared_ptr<WGPUVulkanShared> &wgpuVulkanShared, Window &window)
      : window(window), wgpuVulkanShared(wgpuVulkanShared) {
    createMirrorSurface();
    createMirrorSwapchain();

    fence = wgpuVulkanShared->device.createFence(vk::FenceCreateInfo(), nullptr, wgpuVulkanShared->loader);
  }

  void createMirrorSwapchain(VkSwapchainKHR oldSwapchain = nullptr) {
    assert(mirrorSurface);

    auto &loader = wgpuVulkanShared->loader;

    currentSize = window.getDrawableSize();

    auto surfaceFormats = wgpuVulkanShared->physicalDevice.getSurfaceFormatsKHR(mirrorSurface, loader);
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

    if (!selectedFormatOption)
      throw std::runtime_error("No supported surface formats found");
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
    mirrorSwapchain = wgpuVulkanShared->device.createSwapchainKHR(scInfo, nullptr, loader);

    if (oldSwapchain)
      wgpuVulkanShared->device.destroySwapchainKHR(oldSwapchain, nullptr, loader);

    if (!mirrorSwapchain)
      throw std::runtime_error("Failed to create swapchain");

    // [t] Retrieve the new swapchain images
    mirrorSwapchainImages = wgpuVulkanShared->device.getSwapchainImagesKHR(mirrorSwapchain, loader); 

    mirrorTextureViews.clear();
    for (auto &image : mirrorSwapchainImages) {
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
          .image = image,
      };
      WGPUExternalTextureDescriptor extDesc{
          .nextInChain = &extDescVk.chain,
      }; 

      WGPUTextureView textureView = wgpuCreateExternalTextureView(wgpuVulkanShared->wgpuDevice, &textureDesc, &viewDesc, &extDesc);
      assert(textureView);
      mirrorTextureViews.emplace_back(toWgpuHandle(textureView));
    }
  }

  void createMirrorSurface() {
#if GFX_WINDOWS
    vk::Win32SurfaceCreateInfoKHR surfInfo;
    surfInfo.setHwnd(HWND(window.getNativeWindowHandle()));
    mirrorSurface = wgpuVulkanShared->instance.createWin32SurfaceKHR(surfInfo, nullptr, wgpuVulkanShared->loader);
    if (!mirrorSurface)
      throw std::runtime_error("Failed to create surface");
#else
    static_assert("Platform surface not implemented");
#endif

    auto result =
        wgpuVulkanShared->physicalDevice.getSurfaceSupportKHR(wgpuVulkanShared->queueFamilyIndex, mirrorSurface, wgpuVulkanShared->loader);
    if (!result)
      throw std::runtime_error("Unsupported surface");
  }

  const int2 &getSize() const override { return currentSize; }

  WGPUTextureFormat getFormat() const override { return currentFormat; }

  WGPUTextureView requestFrame() override {
    assert(!currentView);

    vk::ResultValue<uint32_t> result =
        wgpuVulkanShared->device.acquireNextImageKHR(mirrorSwapchain, UINT64_MAX, {}, fence, wgpuVulkanShared->loader);
    if (result.result == vk::Result::eSuccess) {
      vk::Result waitResult = wgpuVulkanShared->device.waitForFences(fence, true, UINT64_MAX, wgpuVulkanShared->loader);
      assert(waitResult == vk::Result::eSuccess);

      wgpuVulkanShared->device.resetFences(fence, wgpuVulkanShared->loader);

      currentImageIndex = result.value; 
      currentView = mirrorTextureViews[currentImageIndex];
      return currentView;
    } else {
      return nullptr;
    }
  }

  WGPUTextureView getCurrentFrame() const override {
    assert(currentView);
    return currentView;
  }

  void render(Headset headset, uint32_t eyeIndex, uint32_t swapchainImageIndex){
    const VkCommandBuffer commandBuffer = renderer->getCurrentCommandBuffer();//[t] TODO: but we want wgpu command buffer, right?
    
    const VkImage sourceImage = headset->getRenderTarget(swapchainImageIndex)->getImage();
    const VkImage destinationImage = mirrorSwapchainImages.at(currentImageIndex); // [t] we have these from createMirrorSwapchain()
    const VkExtent2D eyeResolution = headset->getEyeResolution(eyeIndex);

    // [t] For vulkan, the steps should be smth like this:
    /*
      Convert source image from VK_IMAGE_LAYOUT_UNDEFINED to VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
        vkCmdPipelineBarrier with command buffer

      Convert destination image from VK_IMAGE_LAYOUT_UNDEFINED to VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
        vkCmdPipelineBarrier with command buffer

      match source image eye aspect ratio to window / crop

      VkImageBlit vkCmdBlitImage the source image to destination image

      Convert source image from VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL to VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL

      Convert destination image from VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL to VK_IMAGE_LAYOUT_PRESENT_SRC_KHR

      present()

    */
  }

  void present() override {
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
    vk::Result result = wgpuVulkanShared->queue.presentKHR(&presentInfo, wgpuVulkanShared->loader);
    assert(result == vk::Result::eSuccess);

    // Increment image
    currentView = nullptr;
  }
};

// NOTE: Prototype OpenXR binding code. Just for vulkan and wgpu binding, so gfx only. 
// Does not touch openXR. Needed to create OpenXR swapchains etc. for the xr Headset.cpp.
// The openxr vulkan extensions requires the following:
// - Specific extensions on instance creation
// - Specific physical device as returned by OpenXR
// - Specific extensions on device creation
// - Need to retrieve the created instance, device handles and queue indices
struct ContextXrGfxBackend : public IContextBackend { 
  WGPUSurface wgpuSurface{};

  std::shared_ptr<WGPUVulkanShared> wgpuVulkanShared = std::make_shared<WGPUVulkanShared>();

  //[T] Called by context.cpp 
  WGPUInstance wgpuVkCreateInstance() override 
  {
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
    if (!wgpuVulkanShared->wgpuInstance) 
      throw std::runtime_error("Failed to create WGPUInstance");

    WGPUInstanceProperties props{};
    WGPUInstancePropertiesVK propsVk{
        .chain = {.sType = WGPUSType(WGPUNativeSTypeEx_InstancePropertiesVK)},
    };
    if (getDefaultWgpuBackendType() == WGPUBackendType_Vulkan) {
      props.nextInChain = &propsVk.chain;
    }
    wgpuInstanceGetPropertiesEx(wgpuVulkanShared->wgpuInstance, &props); 

    wgpuVulkanShared->loader = vk::DispatchLoaderDynamic(PFN_vkGetInstanceProcAddr(propsVk.getInstanceProcAddr));

    wgpuVulkanShared->instance = vk::Instance(VkInstance(propsVk.instance));
    wgpuVulkanShared->loader.init(wgpuVulkanShared->instance);

    auto devices = wgpuVulkanShared->instance.enumeratePhysicalDevices(wgpuVulkanShared->loader);
    for (auto &device : devices) {
      auto properties = device.getProperties(wgpuVulkanShared->loader);
      auto name = std::string(properties.deviceName.begin(), properties.deviceName.end());
      SPDLOG_INFO("vulkan physical device: {}", name);
      wgpuVulkanShared->physicalDevice = device;
      break;
    }
    assert(wgpuVulkanShared->physicalDevice);

    return wgpuVulkanShared->wgpuInstance;
  }

  WGPUSurface createSurface(Window &window, void *overrideNativeWindowHandle) override
  {
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
    WGPURequestAdapterOptionsVK optionsVk{
        .chain = {.sType = WGPUSType(WGPUNativeSTypeEx_RequestAdapterOptionsVK)},
        .physicalDevice = wgpuVulkanShared->physicalDevice,
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
    wgpuVulkanShared->wgpuDevice = inDevice;
    wgpuVulkanShared->wgpuQueue = wgpuDeviceGetQueue(wgpuVulkanShared->wgpuDevice);

    WGPUDevicePropertiesVK propsVk{
        .chain = {.sType = WGPUSType(WGPUNativeSTypeEx_DevicePropertiesVK)},
    };
    WGPUDeviceProperties props{.nextInChain = &propsVk.chain};
    wgpuDeviceGetPropertiesEx(wgpuVulkanShared->wgpuDevice, &props);

    wgpuVulkanShared->device = vk::Device(VkDevice(propsVk.device)); 
    wgpuVulkanShared->queue = vk::Queue(VkQueue(propsVk.queue));
    wgpuVulkanShared->queueIndex = propsVk.queueIndex;
    wgpuVulkanShared->queueFamilyIndex = propsVk.queueFamilyIndex;
  }

  std::shared_ptr<DeviceRequest> requestDevice() override {
    std::vector<const char *> requiredExtensions = {};

    WGPUDeviceDescriptorVK deviceDescVk{
        .chain = {.sType = WGPUSType(WGPUNativeSTypeEx_DeviceDescriptorVK)},
        .requiredExtensions = requiredExtensions.data(),
        .requiredExtensionCount = (uint32_t)requiredExtensions.size(),
    };

    WGPUDeviceDescriptor deviceDesc = {};
    deviceDesc.nextInChain = &deviceDescVk.chain;

    WGPURequiredLimits requiredLimits = {.limits = wgpuGetDefaultLimits()};
    deviceDesc.requiredLimits = &requiredLimits;

    auto cb = [&](WGPURequestDeviceStatus status, WGPUDevice device, const char *msg) { deviceCreated(device); };
    return DeviceRequest::create(wgpuVulkanShared->wgpuAdapter, deviceDesc, DeviceRequest::Callbacks{cb});
  }

  std::shared_ptr<WGPUVulkanShared> getWgpuVulkanShared() const {
    
    return wgpuVulkanShared; 
  }

  std::shared_ptr<IContextMainOutput> createMainOutput(Window &window) override {
    // return std::make_shared<ContextWindowOutput>(wgpuInstance, wgpuAdapter, wgpuDevice, wgpuSurface, window);
    return std::make_shared<VulkanOpenXRSwapchain>(wgpuVulkanShared, window);
  }


};
} // namespace gfx

#endif /* A44AEEB5_7CD7_4AAC_A2DE_988863BD5160 */
