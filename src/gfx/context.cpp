#include "context.hpp"
#include "context_data.hpp"
#include "error_utils.hpp"
#include "platform.hpp"
#include "platform_surface.hpp"
#include "window.hpp"
#include <magic_enum.hpp>
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
#elif GFX_LINUX || GFX_ANDROID
  return WGPUBackendType_Vulkan;
#elif GFX_EMSCRIPTEN
  return WGPUBackendType_WebGPU;
#else
#error "No graphics backend defined for platform"
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
  ~ContextMainOutput() {
    releaseSwapchain();
    releaseSurface();
  }

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

  bool acquireFrame() {
    assert(!currentView);
    currentView = wgpuSwapChainGetCurrentTextureView(wgpuSwapchain);

    return currentView;
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

  void initSwapchain(WGPUAdapter adapter, WGPUDevice device) {
    swapchainFormat = wgpuSurfaceGetPreferredFormat(wgpuWindowSurface, adapter);
    int2 mainOutputSize = window->getDrawableSize();
    resizeSwapchain(device, adapter, mainOutputSize);
  }

  void resizeSwapchain(WGPUDevice device, WGPUAdapter adapter, const int2 &newSize) {
    WGPUTextureFormat preferredFormat = wgpuSurfaceGetPreferredFormat(wgpuWindowSurface, adapter);
    if (preferredFormat != swapchainFormat) {
      spdlog::debug("GFX.Context swapchain preferred format changed: {}", magic_enum::enum_name(preferredFormat));
      swapchainFormat = preferredFormat;
    }

    assert(newSize.x > 0 && newSize.y > 0);
    assert(device);
    assert(wgpuWindowSurface);
    assert(swapchainFormat != WGPUTextureFormat_Undefined);

    spdlog::debug("GFX.Context resized width: {} height: {}", newSize.x, newSize.y);
    currentSize = newSize;

    releaseSwapchain();

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

  void releaseSurface() { WGPU_SAFE_RELEASE(wgpuSurfaceRelease, wgpuWindowSurface); }
  void releaseSwapchain() { WGPU_SAFE_RELEASE(wgpuSwapChainRelease, wgpuSwapchain); }
};

Context::Context() {}
Context::~Context() { release(); }

void Context::init(Window &window, const ContextCreationOptions &inOptions) {
  options = inOptions;
  mainOutput = std::make_shared<ContextMainOutput>(window);

  initCommon();
}

void Context::init(const ContextCreationOptions &inOptions) {
  options = inOptions;

  initCommon();
}

void Context::release() {
  spdlog::debug("GFX.Context release");
  state = ContextState::Uninitialized;

  releaseAdapter();
  mainOutput.reset();
}

Window &Context::getWindow() {
  assert(mainOutput);
  return *mainOutput->window;
}

void Context::resizeMainOutputConditional(const int2 &newSize) {
  if (state == ContextState::Incomplete)
    return;

  assert(mainOutput);
  if (mainOutput->currentSize != newSize) {
    mainOutput->resizeSwapchain(wgpuDevice, wgpuAdapter, newSize);
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
  assert(!ptr.expired());

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
      obj.first->releaseContextDataConditional();
    }
  }
}

bool Context::beginFrame() {
  assert(frameState == ContextFrameState::Ok);
  assert(state != ContextState::Uninitialized);

  if (suspended)
    return false;

  if (state == ContextState::Incomplete) {
    if (!tryAcquireDevice())
      return false;
  }

  collectContextData();
  if (!isHeadless()) {
    if (!mainOutput->acquireFrame())
      return false;
  }

  frameState = ContextFrameState::WaitingForEnd;

  return true;
}

void Context::endFrame() {
  assert(frameState == ContextFrameState::WaitingForEnd);

  if (!isHeadless())
    present();

  frameState = ContextFrameState::Ok;
}

void Context::sync() {
#ifdef WEBGPU_NATIVE
  wgpuDevicePoll(wgpuDevice, true);
#endif
}

void Context::suspend() {
#if GFX_ANDROID
  // Also release the surface on suspend
  deviceLost();

  if (mainOutput)
    mainOutput->releaseSurface();
#endif

  suspended = true;
}

void Context::resume() { suspended = false; }

void Context::submit(WGPUCommandBuffer cmdBuffer) { wgpuQueueSubmit(wgpuQueue, 1, &cmdBuffer); }

void Context::deviceLost() {
  if (state != ContextState::Incomplete) {
    spdlog::debug("Device lost");
    state = ContextState::Incomplete;

    releaseDevice();
  }
}

void Context::acquireDevice() {
  assert(!wgpuDevice);
  assert(!wgpuQueue);

  try {
    if (!wgpuAdapter) {
      createAdapter();
    }

    state = ContextState::Ok;

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

    auto errorCallback = [](WGPUErrorType type, char const *message, void *userdata) {
      Context &context = *(Context *)userdata;
      std::string msgString(message);
      spdlog::error("WEBGPU: {} ({})", message, type);
      if (type == WGPUErrorType_DeviceLost) {
        context.deviceLost();
      }
    };
    wgpuDeviceSetUncapturedErrorCallback(wgpuDevice, errorCallback, this);
    wgpuQueue = wgpuDeviceGetQueue(wgpuDevice);

    if (mainOutput) {
      getOrCreateSurface();
      mainOutput->initSwapchain(wgpuAdapter, wgpuDevice);
    }

    WGPUDeviceLostCallback deviceLostCallback = [](WGPUDeviceLostReason reason, char const *message, void *userdata) {
      spdlog::warn("Device lost: {} ()", message, magic_enum::enum_name(reason));
    };
    wgpuDeviceSetDeviceLostCallback(wgpuDevice, deviceLostCallback, this);
  } catch (...) {
    deviceLost();
    throw;
  }

  state = ContextState::Ok;
}

bool Context::tryAcquireDevice() {
  try {
    acquireDevice();
  } catch (std::exception &ex) {
    spdlog::error("Failed to acquire device: {}", ex.what());
    return false;
  }
  return true;
}

void Context::releaseDevice() {
  releaseAllContextData();

  if (mainOutput) {
    mainOutput->releaseSwapchain();
  }

  WGPU_SAFE_RELEASE(wgpuQueueRelease, wgpuQueue);
  WGPU_SAFE_RELEASE(wgpuDeviceRelease, wgpuDevice);
}

WGPUSurface Context::getOrCreateSurface() {
  if (mainOutput)
    return mainOutput->initSurface(wgpuInstance, options.overrideNativeWindowHandle);
  return nullptr;
}

void Context::createAdapter() {
  WGPURequestAdapterOptions requestAdapter = {};
  requestAdapter.powerPreference = WGPUPowerPreference_HighPerformance;
  requestAdapter.compatibleSurface = getOrCreateSurface();
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
}

void Context::releaseAdapter() {
  releaseDevice();
  WGPU_SAFE_RELEASE(wgpuAdapterRelease, wgpuAdapter);
}

void Context::initCommon() {
  spdlog::debug("GFX.Context init");

  assert(!isInitialized());

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
    wgpuSetLogLevel(WGPULogLevel_Info);
  }
#endif

  acquireDevice();
}

void Context::present() {
  assert(mainOutput);
  mainOutput->present();
}

} // namespace gfx
