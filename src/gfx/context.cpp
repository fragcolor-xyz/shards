#include "context.hpp"
#include "context_data.hpp"
#include "error_utils.hpp"
#include "platform.hpp"
#include "platform_surface.hpp"
#include "window.hpp"
#include <SDL_events.h>
#include <SDL_video.h>
#include <spdlog/fmt/fmt.h>
#include <spdlog/spdlog.h>
#include <stdexcept>

#if GFX_EMSCRIPTEN
#include <emscripten/html5.h>
#endif

namespace gfx {

static WGPUBackendType getDefaultWgpuBackendType() {
#if GFX_WINDOWS
  return WGPUBackendType_D3D12;
#elif GFX_APPLE
  return WGPUBackendType_Metal;
#elif GFX_LINUX
  return WGPUBackendType_Vulkan;
#endif
}

struct ContextMainOutput {
  Window *window{};
  WGPUSwapChain wgpuSwapchain{};
  WGPUSurface wgpuWindowSurface{};
  WGPUTextureFormat swapchainFormat = WGPUTextureFormat_Undefined;
  int2 currentSize{};
  WGPUTextureView currentView{};

#if GFX_APPLE
  std::unique_ptr<MetalViewContainer> metalViewContainer;
#endif

  ContextMainOutput(Window &window) { this->window = &window; }
  ~ContextMainOutput() { cleanupSwapchain(); }

  WGPUSurface initSurface(WGPUInstance instance, void *overrideNativeWindowHandle) {
    if (!wgpuWindowSurface) {
      void *surfaceHandle = overrideNativeWindowHandle;

#if GFX_APPLE
      if (!surfaceHandle) {
        metalViewContainer = std::make_unique<MetalViewContainer>(window->window);
        surfaceHandle = metalViewContainer->layer;
      }
#endif

      WGPUPlatformSurfaceDescriptor surfDesc(window->window, surfaceHandle);
      wgpuWindowSurface = wgpuInstanceCreateSurface(instance, &surfDesc);
    }

    return wgpuWindowSurface;
  }

  void acquireFrame() {
    assert(!currentView);
    currentView = wgpuSwapChainGetCurrentTextureView(wgpuSwapchain);
  }

  void present() {
    assert(currentView);

#if GFX_EMSCRIPTEN
    auto callback = [](double time, void *userData) -> EM_BOOL {
      spdlog::debug("Animation frame callback {}", time);
      return true;
    };
    spdlog::debug("Animation frame queued");
    emscripten_request_animation_frame(callback, this);
    emscripten_sleep(0);
#else
    wgpuSwapChainPresent(wgpuSwapchain);
#endif

    currentView = nullptr;
  }

  void init(WGPUAdapter adapter, WGPUDevice device) {
    swapchainFormat = wgpuSurfaceGetPreferredFormat(wgpuWindowSurface, adapter);
    int2 mainOutputSize = window->getDrawableSize();
    resizeSwapchain(device, mainOutputSize);
  }

  void resizeSwapchain(WGPUDevice device, const int2 &newSize) {
    assert(newSize.x > 0 && newSize.y > 0);
    assert(device);
    assert(wgpuWindowSurface);
    assert(swapchainFormat != WGPUTextureFormat_Undefined);

    spdlog::debug("GFX.Context resized width: {} height: {}", newSize.x, newSize.y);
    currentSize = newSize;

    cleanupSwapchain();

    WGPUSwapChainDescriptor swapchainDesc = {};
    swapchainDesc.format = swapchainFormat;
    swapchainDesc.width = newSize.x;
    swapchainDesc.height = newSize.y;
    swapchainDesc.presentMode = WGPUPresentMode_Fifo;
    swapchainDesc.usage = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_CopyDst;
    wgpuSwapchain = wgpuDeviceCreateSwapChain(device, wgpuWindowSurface, &swapchainDesc);
    if (!wgpuSwapchain) {
      throw formatException("Failed to create swapchain");
    }
  }

  void cleanupSwapchain() { WGPU_SAFE_RELEASE(wgpuSwapChainRelease, wgpuSwapchain); }
};

Context::Context() {}
Context::~Context() { cleanup(); }

void Context::init(Window &window, const ContextCreationOptions &options) {
  mainOutput = std::make_shared<ContextMainOutput>(window);
  mainOutput->initSurface(wgpuInstance, options.overrideNativeWindowHandle);

  initCommon(options);
}

void Context::init(const ContextCreationOptions &options) { initCommon(options); }

void Context::initCommon(const ContextCreationOptions &options) {
  spdlog::debug("GFX.Context init");

  assert(!isInitialized());

  WGPURequestAdapterOptions requestAdapter = {};
  requestAdapter.powerPreference = WGPUPowerPreference_HighPerformance;
  requestAdapter.compatibleSurface = mainOutput ? mainOutput->wgpuWindowSurface : nullptr;
  requestAdapter.forceFallbackAdapter = false;

#ifdef WEBGPU_NATIVE
  WGPUAdapterExtras adapterExtras = {};
  requestAdapter.nextInChain = &adapterExtras.chain;
  adapterExtras.chain.sType = (WGPUSType)WGPUSType_AdapterExtras;
  adapterExtras.backend = getDefaultWgpuBackendType();
#endif

  WGPUAdapterReceiverData adapterReceiverData = {};
  wgpuAdapter = wgpuInstanceRequestAdapterSync(wgpuInstance, &requestAdapter, &adapterReceiverData);
  if (adapterReceiverData.status != WGPURequestAdapterStatus_Success) {
    throw formatException("Failed to create wgpuAdapter: {} {}", adapterReceiverData.status, adapterReceiverData.message);
  }
  spdlog::debug("Created wgpuAdapter");

#ifdef WEBGPU_NATIVE
  wgpuSetLogCallback([](WGPULogLevel level, const char *msg) {
    switch (level) {
    case WGPULogLevel_Error:
      spdlog::error("WEBGPU: {}", msg);
      break;
    case WGPULogLevel_Warn:
      spdlog::warn("WEBGPU: {}", msg);
      break;
    case WGPULogLevel_Info:
      spdlog::info("WEBGPU: {}", msg);
      break;
    case WGPULogLevel_Debug:
      spdlog::debug("WEBGPU: {}", msg);
      break;
    case WGPULogLevel_Trace:
      spdlog::trace("WEBGPU: {}", msg);
      break;
    default:
      break;
    }
  });

  if (options.debug) {
    wgpuSetLogLevel(WGPULogLevel_Debug);
  } else {
    wgpuSetLogLevel(WGPULogLevel_Error);
  }
#endif

  WGPURequiredLimits requiredLimits{
      .limits = wgpuGetUndefinedLimits(),
  };

  WGPUDeviceDescriptor deviceDesc = {};
  deviceDesc.requiredLimits = &requiredLimits;

#ifdef WEBGPU_NATIVE
  WGPUDeviceExtras deviceExtras = {};
  deviceDesc.nextInChain = &deviceExtras.chain;
#endif

  WGPUDeviceReceiverData deviceReceiverData = {};
  wgpuDevice = wgpuAdapterRequestDeviceSync(wgpuAdapter, &deviceDesc, &deviceReceiverData);
  if (deviceReceiverData.status != WGPURequestDeviceStatus_Success) {
    throw formatException("Failed to create device: {} {}", deviceReceiverData.status, deviceReceiverData.message);
  }
  spdlog::debug("Create wgpuDevice");

  wgpuDeviceSetUncapturedErrorCallback(
      wgpuDevice,
      [](WGPUErrorType type, char const *message, void *userdata) {
        spdlog::error("WEBGPU: {} ({})", message, type);
        ;
      },
      this);

  wgpuQueue = wgpuDeviceGetQueue(wgpuDevice);

  if (mainOutput) {
    mainOutput->init(wgpuAdapter, wgpuDevice);
  }

  initialized = true;
}

void Context::cleanup() {
  spdlog::debug("GFX.Context cleanup");
  initialized = false;

  releaseAllContextData();

  mainOutput.reset();
  WGPU_SAFE_RELEASE(wgpuQueueRelease, wgpuQueue);
  WGPU_SAFE_RELEASE(wgpuDeviceRelease, wgpuDevice);
  wgpuAdapter = nullptr; // TODO: drop once c binding exists
}

Window &Context::getWindow() {
  assert(mainOutput);
  return *mainOutput->window;
}

void Context::resizeMainOutputConditional(const int2 &newSize) {
  assert(mainOutput);
  if (mainOutput->currentSize != newSize) {
    mainOutput->resizeSwapchain(wgpuDevice, newSize);
  }
}

int2 Context::getMainOutputSize() const {
  assert(mainOutput);
  return mainOutput->currentSize;
}

WGPUTextureView Context::getMainOutputTextureView() {
  assert(mainOutput);
  assert(mainOutput->wgpuSwapchain);
  return mainOutput->currentView;
}

WGPUTextureFormat Context::getMainOutputFormat() const {
  assert(mainOutput);
  return mainOutput->swapchainFormat;
}

bool Context::isHeadless() const { return !mainOutput; }

void Context::addContextDataInternal(std::weak_ptr<ContextData> ptr) {
  std::shared_ptr<ContextData> sharedPtr = ptr.lock();
  if (sharedPtr) {
    contextDatas.insert_or_assign(sharedPtr.get(), ptr);
  }
}

void Context::removeContextDataInternal(ContextData *ptr) { contextDatas.erase(ptr); }

void Context::collectContextData() {
  for (auto it = contextDatas.begin(); it != contextDatas.end();) {
    if (it->second.expired()) {
      it = contextDatas.erase(it);
    } else {
      it++;
    }
  }
}

void Context::releaseAllContextData() {
  auto contextDatas = std::move(this->contextDatas);
  for (auto &obj : contextDatas) {
    if (!obj.second.expired()) {
      obj.first->releaseConditional();
    }
  }
}

void Context::beginFrame() {
  collectContextData();
  if (!isHeadless())
    mainOutput->acquireFrame();
}

void Context::endFrame() {
  if (!isHeadless())
    present();
}

void Context::sync() {
#ifdef WEBGPU_NATIVE
  wgpuDevicePoll(wgpuDevice, true);
#endif
}

void Context::submit(WGPUCommandBuffer cmdBuffer) { wgpuQueueSubmit(wgpuQueue, 1, &cmdBuffer); }

void Context::present() {
  assert(mainOutput);
  mainOutput->present();
}

} // namespace gfx
