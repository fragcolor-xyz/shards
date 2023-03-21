#include "context.hpp"
#include "context_data.hpp"
#include "error_utils.hpp"
#include "platform.hpp"
#include "platform_surface.hpp"
#include "window.hpp"
#include "log.hpp"
#include <SDL_video.h>
#include <tracy/Tracy.hpp>
#include <magic_enum.hpp>
#include <spdlog/fmt/fmt.h>
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <SDL_stdinc.h>

#if GFX_EMSCRIPTEN
#include <emscripten/html5.h>
#endif

namespace gfx {
static auto logger = getLogger();
static auto wgpuLogger = getWgpuLogger();

static WGPUTextureFormat getDefaultSrgbBackbufferFormat() {
#if GFX_ANDROID
  return WGPUTextureFormat_RGBA8UnormSrgb;
#else
  return WGPUTextureFormat_BGRA8UnormSrgb;
#endif
}

#ifdef WEBGPU_NATIVE
static WGPUBackendType getDefaultWgpuBackendType() {
#if GFX_WINDOWS
  // Vulkan is more performant on windows for now, see:
  // https://github.com/gfx-rs/wgpu/issues/2719 - Make DX12 the Default API on Windows
  // https://github.com/gfx-rs/wgpu/issues/2720 - Suballocate Buffers in DX12
  return WGPUBackendType_Vulkan;
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
#endif

struct AdapterRequest {
  typedef AdapterRequest Self;

  WGPURequestAdapterStatus status;
  WGPUAdapter adapter;
  bool finished{};
  std::string message;

  static std::shared_ptr<Self> create(WGPUInstance wgpuInstance, const WGPURequestAdapterOptions &options) {
    auto result = std::make_shared<Self>();
    wgpuInstanceRequestAdapter(wgpuInstance, &options, (WGPURequestAdapterCallback)&Self::callback,
                               new std::shared_ptr<Self>(result));
    return result;
  };

  static void callback(WGPURequestAdapterStatus status, WGPUAdapter adapter, char const *message, std::shared_ptr<Self> *handle) {
    (*handle)->status = status;
    (*handle)->adapter = adapter;
    if (message)
      (*handle)->message = message;
    (*handle)->finished = true;
    delete handle;
  };
};

struct DeviceRequest {
  typedef DeviceRequest Self;

  WGPURequestDeviceStatus status;
  WGPUDevice device;
  bool finished{};
  std::string message;

  static std::shared_ptr<Self> create(WGPUAdapter wgpuAdapter, const WGPUDeviceDescriptor &deviceDesc) {
    auto result = std::make_shared<Self>();
    wgpuAdapterRequestDevice(wgpuAdapter, &deviceDesc, (WGPURequestDeviceCallback)&Self::callback,
                             new std::shared_ptr<Self>(result));
    return result;
  };

  static void callback(WGPURequestDeviceStatus status, WGPUDevice device, char const *message, std::shared_ptr<Self> *handle) {
    (*handle)->status = status;
    (*handle)->device = device;
    if (message)
      (*handle)->message = message;
    (*handle)->finished = true;
    delete handle;
  };
};

struct ContextMainOutput {
  Window *window{};
  WGPUSwapChain wgpuSwapchain{};
  WGPUSurface wgpuWindowSurface{};
  WGPUTextureFormat swapchainFormat = WGPUTextureFormat_Undefined;
  int2 currentSize{};
  WGPUTextureView currentView{};

  ContextMainOutput(Window &window) { this->window = &window; }
  ~ContextMainOutput() {
    releaseSwapchain();
    releaseSurface();
  }

  WGPUSurface initSurface(WGPUInstance instance, void *overrideNativeWindowHandle) {
    ZoneScoped;

    if (!wgpuWindowSurface) {
      void *surfaceHandle = overrideNativeWindowHandle;

#if GFX_APPLE
      if (!surfaceHandle) {
        surfaceHandle = window->metalView->layer;
      }
#endif

      WGPUPlatformSurfaceDescriptor surfDesc(window->window, surfaceHandle);
      wgpuWindowSurface = wgpuInstanceCreateSurface(instance, &surfDesc);
    }

    return wgpuWindowSurface;
  }

  bool requestFrame(WGPUDevice device, WGPUAdapter adapter) {
    assert(!currentView);

    int2 drawableSize = window->getDrawableSize();
    if (drawableSize != currentSize)
      resizeSwapchain(device, adapter, drawableSize);

    currentView = wgpuSwapChainGetCurrentTextureView(wgpuSwapchain);

    return currentView;
  }

  void present() {
    assert(currentView);

    wgpuTextureViewRelease(currentView);
    currentView = nullptr;

    // Web doesn't have a swapchain, it automatically present the current texture when control
    // is returned to the browser
#ifdef WEBGPU_NATIVE
    wgpuSwapChainPresent(wgpuSwapchain);
#endif

    FrameMark;
  }

  void initSwapchain(WGPUDevice device, WGPUAdapter adapter) {
    swapchainFormat = wgpuSurfaceGetPreferredFormat(wgpuWindowSurface, adapter);
    int2 mainOutputSize = window->getDrawableSize();
    resizeSwapchain(device, adapter, mainOutputSize);
  }

  void resizeSwapchain(WGPUDevice device, WGPUAdapter adapter, const int2 &newSize) {
    WGPUTextureFormat preferredFormat = wgpuSurfaceGetPreferredFormat(wgpuWindowSurface, adapter);

    if (preferredFormat == WGPUTextureFormat_Undefined) {
      releaseSwapchain();
      throw formatException("Failed to create swapchain");
    }

    if (preferredFormat != swapchainFormat) {
      SPDLOG_LOGGER_DEBUG(logger, "swapchain preferred format changed: {}", magic_enum::enum_name(preferredFormat));
      swapchainFormat = preferredFormat;
    }

    assert(newSize.x > 0 && newSize.y > 0);
    assert(device);
    assert(wgpuWindowSurface);
    assert(swapchainFormat != WGPUTextureFormat_Undefined);

    SPDLOG_LOGGER_DEBUG(logger, "resized width: {} height: {}", newSize.x, newSize.y);
    currentSize = newSize;

    releaseSwapchain();

    WGPUSwapChainDescriptor swapchainDesc = {};
    swapchainDesc.format = swapchainFormat;

    // Canvas size should't be set when configuring, instead resize the element
    // https://github.com/emscripten-core/emscripten/issues/17416
#if !GFX_EMSCRIPTEN
    swapchainDesc.width = newSize.x;
    swapchainDesc.height = newSize.y;
#endif

#if GFX_WINDOWS || GFX_OSX || GFX_LINUX
    swapchainDesc.presentMode = WGPUPresentMode_Immediate;
#else
    swapchainDesc.presentMode = WGPUPresentMode_Fifo;
#endif
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
  SPDLOG_LOGGER_DEBUG(logger, "release");
  state = ContextState::Uninitialized;

  releaseAdapter();
  mainOutput.reset();

  WGPU_SAFE_RELEASE(wgpuInstanceRelease, wgpuInstance);
}

Window &Context::getWindow() {
  assert(mainOutput);
  return *mainOutput->window;
}

void Context::resizeMainOutputConditional(const int2 &newSize) {
  ZoneScoped;

  if (state != ContextState::Ok)
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

void Context::addContextDataInternal(const std::weak_ptr<ContextData> &ptr) {
  ZoneScoped;
  assert(!ptr.expired());

  std::shared_ptr<ContextData> sharedPtr = ptr.lock();
  if (sharedPtr) {
    std::scoped_lock<std::shared_mutex> _lock(contextDataLock);
    contextDatas.insert_or_assign(sharedPtr.get(), ptr);
  }
}

void Context::removeContextDataInternal(ContextData *ptr) {
  std::scoped_lock<std::shared_mutex> _lock(contextDataLock);
  contextDatas.erase(ptr);
}

void Context::collectContextData() {
  std::scoped_lock<std::shared_mutex> _lock(contextDataLock);
  for (auto it = contextDatas.begin(); it != contextDatas.end();) {
    if (it->second.expired()) {
      it = contextDatas.erase(it);
    } else {
      it++;
    }
  }
}

void Context::releaseAllContextData() {
  contextDataLock.lock();
  auto contextDatas = std::move(this->contextDatas);
  contextDataLock.unlock();
  for (auto &obj : contextDatas) {
    if (!obj.second.expired()) {
      obj.first->releaseContextDataConditional();
    }
  }
}

bool Context::beginFrame() {
  ZoneScoped;
  assert(frameState == ContextFrameState::Ok);

  // Automatically request
  if (state == ContextState::Incomplete) {
    requestDevice();
  }

  if (!isReady())
    tickRequesting();

  if (state != ContextState::Ok)
    return false;

  if (suspended)
    return false;

  collectContextData();
  if (!isHeadless()) {
    const int maxAttempts = 2;
    bool success = false;

    // Try to request the swapchain texture, automatically recreate swapchain on failure
    for (size_t i = 0; !success && i < maxAttempts; i++) {
      success = mainOutput->requestFrame(wgpuDevice, wgpuAdapter);
      if (!success) {
        SPDLOG_LOGGER_INFO(logger, "Failed to get current swapchain texture, forcing recreate");
        mainOutput->initSwapchain(wgpuDevice, wgpuAdapter);
      }
    }

    if (!success)
      return false;
  }

  frameState = ContextFrameState::WaitingForEnd;

  return true;
}

void Context::endFrame() {
  ZoneScoped;
  assert(frameState == ContextFrameState::WaitingForEnd);

  if (!isHeadless())
    present();

  frameState = ContextFrameState::Ok;
}

void Context::poll(bool wait) {
  ZoneScoped;
#ifdef WEBGPU_NATIVE
  wgpuDevicePoll(wgpuDevice, wait, nullptr);
#else
  emscripten_sleep(0);
#endif
}

void Context::suspend() {
#if GFX_ANDROID
  // Also release the surface on suspend
  deviceLost();

  if (mainOutput)
    mainOutput->releaseSurface();
#elif GFX_APPLE
  deviceLost();
#endif

  suspended = true;
}

void Context::resume() { suspended = false; }

void Context::submit(WGPUCommandBuffer cmdBuffer) {
  ZoneScoped;
  wgpuQueueSubmit(wgpuQueue, 1, &cmdBuffer);
}

void Context::deviceLost() {
  if (state != ContextState::Incomplete) {
    SPDLOG_LOGGER_DEBUG(logger, "Device lost");
    state = ContextState::Incomplete;

    releaseDevice();
  }
}

void Context::tickRequesting() {
  ZoneScoped;
  SPDLOG_LOGGER_DEBUG(logger, "tickRequesting");
  try {
    if (adapterRequest) {
      if (adapterRequest->finished) {
        if (adapterRequest->status != WGPURequestAdapterStatus_Success) {
          throw formatException("Failed to create adapter: {} {}", magic_enum::enum_name(adapterRequest->status),
                                adapterRequest->message);
        }

        wgpuAdapter = adapterRequest->adapter;
        adapterRequest.reset();

        // Immediately request a device from the adapter
        requestDevice();
      }
    }

    if (deviceRequest) {
      if (deviceRequest->finished) {
        if (deviceRequest->status != WGPURequestDeviceStatus_Success) {
          throw formatException("Failed to create device: {} {}", magic_enum::enum_name(deviceRequest->status),
                                deviceRequest->message);
        }
        wgpuDevice = deviceRequest->device;
        deviceRequest.reset();
        deviceObtained();
      }
    }
  } catch (...) {
    deviceLost();
    throw;
  }
}

void Context::deviceObtained() {
  ZoneScoped;
  state = ContextState::Ok;
  SPDLOG_LOGGER_DEBUG(logger, "wgpuDevice obtained");

  auto errorCallback = [](WGPUErrorType type, char const *message, void *userdata) {
    Context &context = *(Context *)userdata;
    std::string msgString(message);
    SPDLOG_LOGGER_ERROR(wgpuLogger, "{} ({})", message, type);
    if (type == WGPUErrorType_DeviceLost) {
      context.deviceLost();
    }
  };
  wgpuDeviceSetUncapturedErrorCallback(wgpuDevice, errorCallback, this);
  wgpuQueue = wgpuDeviceGetQueue(wgpuDevice);

  if (mainOutput) {
    getOrCreateSurface();
    mainOutput->initSwapchain(wgpuDevice, wgpuAdapter);
  }

  WGPUDeviceLostCallback deviceLostCallback = [](WGPUDeviceLostReason reason, char const *message, void *userdata) {
    SPDLOG_LOGGER_WARN(logger, "Device lost: {} ()", message, magic_enum::enum_name(reason));
  };
  wgpuDeviceSetDeviceLostCallback(wgpuDevice, deviceLostCallback, this);
}

void Context::requestDevice() {
  ZoneScoped;
  assert(!wgpuDevice);
  assert(!wgpuQueue);

  // Request adapter first
  if (!wgpuAdapter) {
    requestAdapter();
    tickRequesting(); // This chains into another requestDevice call once the adapter is obtained
    return;
  }

  state = ContextState::Requesting;

  WGPUDeviceDescriptor deviceDesc = {};

  // Passed to force full feature set to be enabled
#if WEBGPU_NATIVE
  WGPURequiredLimits requiredLimits = {.limits = wgpuGetDefaultLimits()};
  deviceDesc.requiredLimits = &requiredLimits;

  // Lower default limits to support devices like iOS simulator
  requiredLimits.limits.maxBufferSize = 256 * 1024 * 1024;
  WGPURequiredLimitsExtras extraLimits{
      .chain =
          WGPUChainedStruct{
              .sType = (WGPUSType)WGPUSType_RequiredLimitsExtras,
          },
  };
  requiredLimits.nextInChain = &extraLimits.chain;
#endif

  SPDLOG_LOGGER_DEBUG(logger, "Requesting wgpu device");
  deviceRequest = DeviceRequest::create(wgpuAdapter, deviceDesc);
}

void Context::releaseDevice() {
  ZoneScoped;
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

void Context::requestAdapter() {
  ZoneScoped;
  assert(!wgpuAdapter);

  state = ContextState::Requesting;

  if (!wgpuInstance) {
    WGPUInstanceDescriptor desc{};
    wgpuInstance = wgpuCreateInstance(&desc);
  }

  WGPURequestAdapterOptions requestAdapter = {};
  requestAdapter.powerPreference = WGPUPowerPreference_HighPerformance;
  requestAdapter.compatibleSurface = getOrCreateSurface();
  requestAdapter.forceFallbackAdapter = false;

#ifdef WEBGPU_NATIVE
  WGPUAdapterExtras adapterExtras = {};
  requestAdapter.nextInChain = &adapterExtras.chain;
  adapterExtras.chain.sType = (WGPUSType)WGPUSType_AdapterExtras;

  adapterExtras.backend = WGPUBackendType_Null;
  if (const char *backendStr = SDL_getenv("GFX_BACKEND")) {
    std::string typeStr = std::string("WGPUBackendType_") + backendStr;
    auto foundValue = magic_enum::enum_cast<WGPUBackendType>(typeStr);
    if (foundValue) {
      adapterExtras.backend = foundValue.value();
    }
  }

  if (adapterExtras.backend == WGPUBackendType_Null)
    adapterExtras.backend = getDefaultWgpuBackendType();

  SPDLOG_LOGGER_INFO(logger, "Using backend {}", magic_enum::enum_name(adapterExtras.backend));
#endif

  SPDLOG_LOGGER_DEBUG(logger, "Requesting wgpu adapter");
  adapterRequest = AdapterRequest::create(wgpuInstance, requestAdapter);
}

void Context::releaseAdapter() {
  ZoneScoped;
  releaseDevice();
  WGPU_SAFE_RELEASE(wgpuAdapterRelease, wgpuAdapter);
}

void Context::initCommon() {
  ZoneScoped;
  SPDLOG_LOGGER_DEBUG(logger, "initCommon");

  assert(!isInitialized());

#ifdef WEBGPU_NATIVE
  wgpuSetLogCallback(
      [](WGPULogLevel level, const char *msg, void *userData) {
        (void)userData;
        switch (level) {
        case WGPULogLevel_Error:
          wgpuLogger->error("{}", msg);
          break;
        case WGPULogLevel_Warn:
          wgpuLogger->warn("{}", msg);
          break;
        case WGPULogLevel_Info:
          wgpuLogger->info("{}", msg);
          break;
        case WGPULogLevel_Debug:
          wgpuLogger->debug("{}", msg);
          break;
        case WGPULogLevel_Trace:
          wgpuLogger->trace("{}", msg);
          break;
        default:
          break;
        }
      },
      this);

  if (logger->level() <= spdlog::level::debug) {
    wgpuSetLogLevel(WGPULogLevel_Debug);
  } else {
    wgpuSetLogLevel(WGPULogLevel_Info);
  }
#endif

  requestDevice();
}

void Context::present() {
  ZoneScoped;
  assert(mainOutput);
  mainOutput->present();
}

#if GFX_APPLE
MetalViewContainer &Context::getMetalViewContainer() const { return *mainOutput->metalViewContainer; }
#endif

} // namespace gfx
