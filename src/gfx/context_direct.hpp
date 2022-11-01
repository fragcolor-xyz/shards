#ifndef D95C878A_97DA_4201_8B0C_4234299AA49D
#define D95C878A_97DA_4201_8B0C_4234299AA49D

#include "context_internal.hpp"
#include "window.hpp"
#include "fmt.hpp"
#include <magic_enum.hpp>

namespace gfx {

struct ContextWindowOutput : public IContextMainOutput {
  Window &window;
  WGPUInstance instance;
  WGPUAdapter adapter;
  WGPUDevice device;
  WGPUSurface surface;

  WGPUSwapChain wgpuSwapchain{};
  WGPUTextureFormat swapchainFormat = WGPUTextureFormat_Undefined;
  int2 currentSize{};
  WGPUTextureView currentView{};

#if GFX_APPLE
  std::unique_ptr<MetalViewContainer> metalViewContainer;
#endif

  ContextWindowOutput(WGPUInstance instance, WGPUAdapter adapter, WGPUDevice device, WGPUSurface surface, Window &window)
      : window(window), instance(instance), adapter(adapter), device(device), surface(surface) {
    // Init by calling resize the first time
    resize(window.getDrawableSize());
  }

  ~ContextWindowOutput() { releaseSwapchain(); }

  const int2 &getSize() const override { return currentSize; }
  WGPUTextureFormat getFormat() const override { return swapchainFormat; }

  WGPUTextureView requestFrame() override {
    int2 targetSize = window.getDrawableSize();
    if (targetSize != getSize()) {
      SPDLOG_LOGGER_INFO(logger, "Resizing swapchain, window resized ({} => {})", getSize(), targetSize);
      resize(targetSize);
    }

    assert(!currentView);
    currentView = wgpuSwapChainGetCurrentTextureView(wgpuSwapchain);
    if (!currentView) {
      SPDLOG_LOGGER_INFO(logger, "Failed to get current swapchain texture, trying recreate");
      resize(getSize());
      currentView = wgpuSwapChainGetCurrentTextureView(wgpuSwapchain);
    }

    return currentView;
  }

  WGPUTextureView getCurrentFrame() const override { return currentView; }

  void present() override {
    assert(currentView);

    // Web doesn't have a swapchain, it automatically present the current texture when control
    // is returned to the browser
#ifdef WEBGPU_NATIVE
    wgpuSwapChainPresent(wgpuSwapchain);
#endif

    wgpuTextureViewRelease(currentView);
    currentView = nullptr;
  }

  void resize(const int2 &newSize) {
    assert(adapter);
    assert(device);
    assert(surface);

    WGPUTextureFormat preferredFormat = wgpuSurfaceGetPreferredFormat(surface, adapter);

    // Force the backbuffer to srgb format so we don't have to convert manually in shader
    preferredFormat = getDefaultSrgbBackbufferFormat();

    if (preferredFormat != swapchainFormat) {
      SPDLOG_LOGGER_DEBUG(logger, "swapchain preferred format changed: {}", magic_enum::enum_name(preferredFormat));
      swapchainFormat = preferredFormat;
    }

    assert(newSize.x > 0 && newSize.y > 0);
    assert(swapchainFormat != WGPUTextureFormat_Undefined);

    SPDLOG_LOGGER_DEBUG(logger, "resized width: {} height: {}", newSize.x, newSize.y);
    currentSize = newSize;

    releaseSwapchain();

    WGPUSwapChainDescriptor swapchainDesc = {};
    swapchainDesc.format = swapchainFormat;
    swapchainDesc.width = newSize.x;
    swapchainDesc.height = newSize.y;
#if GFX_WINDOWS || GFX_OSX || GFX_LINUX
    swapchainDesc.presentMode = WGPUPresentMode_Immediate;
#else
    swapchainDesc.presentMode = WGPUPresentMode_Fifo;
#endif
    swapchainDesc.usage = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_CopyDst;
    wgpuSwapchain = wgpuDeviceCreateSwapChain(device, surface, &swapchainDesc);
    if (!wgpuSwapchain) {
      throw formatException("Failed to create swapchain");
    }
  }

  void releaseSwapchain() { WGPU_SAFE_RELEASE(wgpuSwapChainRelease, wgpuSwapchain); }
};

// struct DirectBackend : public IContextBackend {
//   WGPUInstance wgpuInstance{};
//   WGPUAdapter wgpuAdapter{};
//   WGPUDevice wgpuDevice{};
//   WGPUQueue wgpuQueue{};
//   WGPUSurface wgpuSurface{};

//   WGPUInstance createInstance() override {
//     // Create instance
//     WGPUInstanceDescriptor desc{};

//     wgpuInstance = wgpuCreateInstanceEx(&desc);
//     if (!wgpuInstance)
//       throw std::runtime_error("Failed to create WGPUInstance");

//     return wgpuInstance;
//   }

//   WGPUSurface createSurface(Window &window, void *overrideNativeWindowHandle) override {
//     void *surfaceHandle = overrideNativeWindowHandle;

// #if GFX_APPLE
//     if (!surfaceHandle) {
//       metalViewContainer = std::make_unique<MetalViewContainer>(window->window);
//       surfaceHandle = metalViewContainer->layer;
//     }
// #endif

//     WGPUPlatformSurfaceDescriptor surfDesc(window.window, surfaceHandle);
//     wgpuSurface = wgpuInstanceCreateSurface(wgpuInstance, &surfDesc);

//     return wgpuSurface;
//   }

//   std::shared_ptr<AdapterRequest> requestAdapter() override {
//     WGPURequestAdapterOptionsVK optionsVk{
//         .chain = {.sType = WGPUSType(WGPUNativeSTypeEx_RequestAdapterOptionsVK)},
//         .physicalDevice = physicalDeviceToUse,
//     };

//     WGPURequestAdapterOptions options{};
//     options.powerPreference = WGPUPowerPreference_HighPerformance;
//     options.compatibleSurface = wgpuSurface;
//     options.forceFallbackAdapter = false;
//     options.nextInChain = &optionsVk.chain;

//     auto cb = [&](WGPURequestAdapterStatus status, WGPUAdapter adapter, const char *msg) { wgpuAdapter = adapter; };
//     return AdapterRequest::create(wgpuInstance, options, AdapterRequest::Callbacks{cb});
//   }

//   std::shared_ptr<DeviceRequest> requestDevice() override {
//     std::vector<const char *> requiredExtensions = {};

//     WGPUDeviceDescriptorVK deviceDescVk{
//         .chain = {.sType = WGPUSType(WGPUNativeSTypeEx_DeviceDescriptorVK)},
//         .requiredExtensions = requiredExtensions.data(),
//         .requiredExtensionCount = (uint32_t)requiredExtensions.size(),
//     };

//     WGPUDeviceDescriptor deviceDesc = {};
//     deviceDesc.nextInChain = &deviceDescVk.chain;

//     WGPURequiredLimits requiredLimits = {.limits = wgpuGetDefaultLimits()};
//     deviceDesc.requiredLimits = &requiredLimits;

//     auto cb = [&](WGPURequestDeviceStatus status, WGPUDevice device, const char *msg) { wgpuDevice = device; };
//     return DeviceRequest::create(wgpuAdapter, deviceDesc, DeviceRequest::Callbacks{cb});
//   }

//   std::shared_ptr<IContextMainOutput> createMainOutput(Window &window) override {
//     return std::make_shared<VulkanOpenXRSwapchain>(wgpuInstance, wgpuAdapter, wgpuDevice, window);
//   }
// };

} // namespace gfx

#endif /* D95C878A_97DA_4201_8B0C_4234299AA49D */
