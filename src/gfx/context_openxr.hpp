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

namespace gfx {

struct VulkanShared {
  vk::DispatchLoaderDynamic loader;
  vk::PhysicalDevice physicalDevice;
  vk::Instance instance;
  vk::Device device;
  vk::Queue queue;

  uint32_t queueIndex;
  uint32_t queueFamilyIndex;

  WGPUInstance wgpuInstance{};
  WGPUAdapter wgpuAdapter{};
  WGPUDevice wgpuDevice{};
  WGPUQueue wgpuQueue{};
};

struct VulkanOpenXRSwapchain : public IContextMainOutput {
  Window &window;

  std::shared_ptr<VulkanShared> vulkanShared;

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

  VulkanOpenXRSwapchain(std::shared_ptr<VulkanShared> &vulkanShared, Window &window)
      : window(window), vulkanShared(vulkanShared) {
    createMirrorSurface();
    createMirrorSwapchain();

    fence = vulkanShared->device.createFence(vk::FenceCreateInfo(), nullptr, vulkanShared->loader);
  }

  void createMirrorSwapchain(VkSwapchainKHR oldSwapchain = nullptr) {
    assert(mirrorSurface);

    auto &loader = vulkanShared->loader;

    currentSize = window.getDrawableSize();

    auto surfaceFormats = vulkanShared->physicalDevice.getSurfaceFormatsKHR(mirrorSurface, loader);
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
    mirrorSwapchain = vulkanShared->device.createSwapchainKHR(scInfo, nullptr, loader);

    if (oldSwapchain)
      vulkanShared->device.destroySwapchainKHR(oldSwapchain, nullptr, loader);

    if (!mirrorSwapchain)
      throw std::runtime_error("Failed to create swapchain");

    mirrorSwapchainImages = vulkanShared->device.getSwapchainImagesKHR(mirrorSwapchain, loader);

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

      WGPUTextureView textureView = wgpuCreateExternalTextureView(vulkanShared->wgpuDevice, &textureDesc, &viewDesc, &extDesc);
      assert(textureView);
      mirrorTextureViews.emplace_back(toWgpuHandle(textureView));
    }
  }

  void createMirrorSurface() {
#if GFX_WINDOWS
    vk::Win32SurfaceCreateInfoKHR surfInfo;
    surfInfo.setHwnd(HWND(window.getNativeWindowHandle()));
    mirrorSurface = vulkanShared->instance.createWin32SurfaceKHR(surfInfo, nullptr, vulkanShared->loader);
    if (!mirrorSurface)
      throw std::runtime_error("Failed to create surface");
#else
    static_assert("Platform surface not implemented");
#endif

    auto result =
        vulkanShared->physicalDevice.getSurfaceSupportKHR(vulkanShared->queueFamilyIndex, mirrorSurface, vulkanShared->loader);
    if (!result)
      throw std::runtime_error("Unsupported surface");
  }

  const int2 &getSize() const override { return currentSize; }

  WGPUTextureFormat getFormat() const override { return currentFormat; }

  WGPUTextureView requestFrame() override {
    assert(!currentView);

    vk::ResultValue<uint32_t> result =
        vulkanShared->device.acquireNextImageKHR(mirrorSwapchain, UINT64_MAX, {}, fence, vulkanShared->loader);
    if (result.result == vk::Result::eSuccess) {
      vk::Result waitResult = vulkanShared->device.waitForFences(fence, true, UINT64_MAX, vulkanShared->loader);
      assert(waitResult == vk::Result::eSuccess);

      vulkanShared->device.resetFences(fence, vulkanShared->loader);

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

  void present() override {
    WGPUExternalPresentVK wgpuPresent{};
    wgpuPrepareExternalPresentVK(vulkanShared->wgpuQueue, &wgpuPresent);

    vk::Semaphore waitSemaphore = wgpuPresent.waitSemaphore ? VkSemaphore(wgpuPresent.waitSemaphore) : nullptr;

    vk::PresentInfoKHR presentInfo;
    presentInfo.setSwapchainCount(1);
    presentInfo.setPSwapchains(&mirrorSwapchain);
    presentInfo.setPImageIndices(&currentImageIndex);
    presentInfo.setPWaitSemaphores(&waitSemaphore);
    presentInfo.setWaitSemaphoreCount(1);

    // presentInfo.setWaitSemaphores(const vk::ArrayProxyNoTemporaries<const vk::Semaphore> &waitSemaphores_)
    vk::Result result = vulkanShared->queue.presentKHR(&presentInfo, vulkanShared->loader);
    assert(result == vk::Result::eSuccess);

    // Increment image
    currentView = nullptr;
  }
};

// NOTE: Prototype OpenXR binding code
// the openxr vulkan extensions requires the following:
// - Specific extensions on instance creation
// - Specific physical device as returned by OpenXR
// - Specific extensions on device creation
// - Need to retrieve the created instance, device handles and queue indices
struct VulkanOpenXRBackend : public IContextBackend {
  WGPUSurface wgpuSurface{};

  std::shared_ptr<VulkanShared> vulkanShared = std::make_shared<VulkanShared>();

  WGPUInstance createInstance() override {
    // Create instance
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

    vulkanShared->wgpuInstance = wgpuCreateInstanceEx(&desc);
    if (!vulkanShared->wgpuInstance)
      throw std::runtime_error("Failed to create WGPUInstance");

    WGPUInstanceProperties props{};
    WGPUInstancePropertiesVK propsVk{
        .chain = {.sType = WGPUSType(WGPUNativeSTypeEx_InstancePropertiesVK)},
    };
    if (getDefaultWgpuBackendType() == WGPUBackendType_Vulkan) {
      props.nextInChain = &propsVk.chain;
    }
    wgpuInstanceGetPropertiesEx(vulkanShared->wgpuInstance, &props);

    vulkanShared->loader = vk::DispatchLoaderDynamic(PFN_vkGetInstanceProcAddr(propsVk.getInstanceProcAddr));

    vulkanShared->instance = vk::Instance(VkInstance(propsVk.instance));
    vulkanShared->loader.init(vulkanShared->instance);

    auto devices = vulkanShared->instance.enumeratePhysicalDevices(vulkanShared->loader);
    for (auto &device : devices) {
      auto properties = device.getProperties(vulkanShared->loader);
      auto name = std::string(properties.deviceName.begin(), properties.deviceName.end());
      SPDLOG_INFO("vulkan physical device: {}", name);
      vulkanShared->physicalDevice = device;
      break;
    }
    assert(vulkanShared->physicalDevice);

    return vulkanShared->wgpuInstance;
  }

  WGPUSurface createSurface(Window &window, void *overrideNativeWindowHandle) override {
    void *surfaceHandle = overrideNativeWindowHandle;

#if GFX_APPLE
    if (!surfaceHandle) {
      metalViewContainer = std::make_unique<MetalViewContainer>(window->window);
      surfaceHandle = metalViewContainer->layer;
    }
#endif

    WGPUPlatformSurfaceDescriptor surfDesc(window.window, surfaceHandle);
    wgpuSurface = wgpuInstanceCreateSurface(vulkanShared->wgpuInstance, &surfDesc);

    return wgpuSurface;
  }

  std::shared_ptr<AdapterRequest> requestAdapter() override {
    WGPURequestAdapterOptionsVK optionsVk{
        .chain = {.sType = WGPUSType(WGPUNativeSTypeEx_RequestAdapterOptionsVK)},
        .physicalDevice = vulkanShared->physicalDevice,
    };

    WGPURequestAdapterOptions options{};
    options.powerPreference = WGPUPowerPreference_HighPerformance;
    options.compatibleSurface = wgpuSurface;
    options.forceFallbackAdapter = false;
    options.nextInChain = &optionsVk.chain;

    auto cb = [&](WGPURequestAdapterStatus status, WGPUAdapter adapter, const char *msg) { vulkanShared->wgpuAdapter = adapter; };
    return AdapterRequest::create(vulkanShared->wgpuInstance, options, AdapterRequest::Callbacks{cb});
  }

  void deviceCreated(WGPUDevice inDevice) {
    vulkanShared->wgpuDevice = inDevice;
    vulkanShared->wgpuQueue = wgpuDeviceGetQueue(vulkanShared->wgpuDevice);

    WGPUDevicePropertiesVK propsVk{
        .chain = {.sType = WGPUSType(WGPUNativeSTypeEx_DevicePropertiesVK)},
    };
    WGPUDeviceProperties props{.nextInChain = &propsVk.chain};
    wgpuDeviceGetPropertiesEx(vulkanShared->wgpuDevice, &props);

    vulkanShared->device = vk::Device(VkDevice(propsVk.device));
    vulkanShared->queue = vk::Queue(VkQueue(propsVk.queue));
    vulkanShared->queueIndex = propsVk.queueIndex;
    vulkanShared->queueFamilyIndex = propsVk.queueFamilyIndex;
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
    return DeviceRequest::create(vulkanShared->wgpuAdapter, deviceDesc, DeviceRequest::Callbacks{cb});
  }

  std::shared_ptr<IContextMainOutput> createMainOutput(Window &window) override {
    // return std::make_shared<ContextWindowOutput>(wgpuInstance, wgpuAdapter, wgpuDevice, wgpuSurface, window);
    return std::make_shared<VulkanOpenXRSwapchain>(vulkanShared, window);
  }
};
} // namespace gfx

#endif /* A44AEEB5_7CD7_4AAC_A2DE_988863BD5160 */
