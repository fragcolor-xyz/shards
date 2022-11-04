#ifndef A44AEEB5_7CD7_4AAC_A2DE_988863BD5160
#define A44AEEB5_7CD7_4AAC_A2DE_988863BD5160

#include "context_internal.hpp"
#include "context_direct.hpp"
#include "platform_surface.hpp"

namespace gfx {

// NOTE: Prototype OpenXR binding code
// the openxr vulkan extensions requires the following:
// - Specific extensions on instance creation
// - Specific physical device as returned by OpenXR
// - Specific extensions on device creation
// - Need to retrieve the created instance, device handles and queue indices
struct VulkanOpenXRBackend : public IContextBackend {
  WGPUInstance wgpuInstance{};
  WGPUAdapter wgpuAdapter{};
  WGPUDevice wgpuDevice{};
  WGPUQueue wgpuQueue{};
  WGPUSurface wgpuSurface{};

  vk::DispatchLoaderDynamic loader;
  vk::Instance instance;

  VkPhysicalDevice physicalDeviceToUse{};

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

    wgpuInstance = wgpuCreateInstanceEx(&desc);
    if (!wgpuInstance)
      throw std::runtime_error("Failed to create WGPUInstance");

    WGPUInstanceProperties props{};
    WGPUInstancePropertiesVK propsVk{
        .chain = {.sType = WGPUSType(WGPUNativeSTypeEx_InstancePropertiesVK)},
    };
    if (getDefaultWgpuBackendType() == WGPUBackendType_Vulkan) {
      props.nextInChain = &propsVk.chain;
    }
    wgpuInstanceGetPropertiesEx(wgpuInstance, &props);

    loader = vk::DispatchLoaderDynamic(PFN_vkGetInstanceProcAddr(propsVk.getInstanceProcAddr));

    instance = vk::Instance(VkInstance(propsVk.instance));
    loader.init(instance);

    auto devices = instance.enumeratePhysicalDevices(loader);
    for (auto &device : devices) {
      auto properties = device.getProperties(loader);
      auto name = std::string(properties.deviceName.begin(), properties.deviceName.end());
      SPDLOG_INFO("vulkan physical device: {}", name);
      physicalDeviceToUse = device;
      break;
    }
    assert(physicalDeviceToUse);

    return wgpuInstance;
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
    wgpuSurface = wgpuInstanceCreateSurface(wgpuInstance, &surfDesc);

    return wgpuSurface;
  }

  std::shared_ptr<AdapterRequest> requestAdapter() override {
    WGPURequestAdapterOptionsVK optionsVk{
        .chain = {.sType = WGPUSType(WGPUNativeSTypeEx_RequestAdapterOptionsVK)},
        .physicalDevice = physicalDeviceToUse,
    };

    WGPURequestAdapterOptions options{};
    options.powerPreference = WGPUPowerPreference_HighPerformance;
    options.compatibleSurface = wgpuSurface;
    options.forceFallbackAdapter = false;
    options.nextInChain = &optionsVk.chain;

    auto cb = [&](WGPURequestAdapterStatus status, WGPUAdapter adapter, const char *msg) { wgpuAdapter = adapter; };
    return AdapterRequest::create(wgpuInstance, options, AdapterRequest::Callbacks{cb});
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

    auto cb = [&](WGPURequestDeviceStatus status, WGPUDevice device, const char *msg) { wgpuDevice = device; };
    return DeviceRequest::create(wgpuAdapter, deviceDesc, DeviceRequest::Callbacks{cb});
  }

  std::shared_ptr<IContextMainOutput> createMainOutput(Window &window) override {
    return std::make_shared<ContextWindowOutput>(wgpuInstance, wgpuAdapter, wgpuDevice, wgpuSurface, window);
  }
};
} // namespace gfx

#endif /* A44AEEB5_7CD7_4AAC_A2DE_988863BD5160 */
