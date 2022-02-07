#include "context.hpp"
#include "context_data.hpp"
#include "error_utils.hpp"
#include "platform.hpp"
#include "platform_surface.hpp"
#include "sdl_native_window.hpp"
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

void ErrorScope::push(WGPUErrorFilter filter) {
#ifndef WEBGPU_NATIVE
  wgpuDevicePushErrorScope(context->wgpuDevice, filter);
#endif
}
void ErrorScope::pop(ErrorScope::Function &&function) {
  this->function = [=, this](WGPUErrorType type, char const *message) {
    function(type, message);
    processed = true;
  };

#ifndef WEBGPU_NATIVE
  wgpuDevicePopErrorScope(context->wgpuDevice, &staticCallback, this);
#else
  processed = true;
#endif
}
void ErrorScope::staticCallback(WGPUErrorType type, char const *message, void *userData) {
  ErrorScope *self = (ErrorScope *)userData;
  self->function(type, message);
}

struct ContextMainOutput {
  Window *window{};
  WGPUSwapChain wgpuSwapchain{};
  WGPUSurface wgpuWindowSurface{};
  WGPUTextureFormat swapchainFormat = WGPUTextureFormat_Undefined;
  int2 currentSize{};

  ContextMainOutput(Window &window) { this->window = &window; }
  ~ContextMainOutput() { cleanupSwapchain(); }

  WGPUSurface initSurface(WGPUInstance instance, void *overrideNativeWindowHandle) {
    if (!wgpuWindowSurface) {
      void *surfaceHandle = overrideNativeWindowHandle;
      if (!surfaceHandle)
        surfaceHandle = SDL_GetNativeWindowPtr(window->window);

      WGPUPlatformSurfaceDescriptor surfDesc(surfaceHandle);
      wgpuWindowSurface = wgpuInstanceCreateSurface(instance, &surfDesc);
    }

    return wgpuWindowSurface;
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
  adapterExtras.backend = WGPUBackendType_D3D12;
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
  return wgpuSwapChainGetCurrentTextureView(mainOutput->wgpuSwapchain);
}

WGPUTextureFormat Context::getMainOutputFormat() const {
  assert(mainOutput);
  return mainOutput->swapchainFormat;
}

bool Context::isHeadless() const { return !mainOutput; }

ErrorScope &Context::pushErrorScope(WGPUErrorFilter filter) {
  auto errorScope = std::make_shared<ErrorScope>(this);
  errorScopes.push_back(errorScope);
  errorScope->push(filter);
  return *errorScope.get();
}

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
  errorScopes.clear();
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

void Context::submit(WGPUCommandBuffer cmdBuffer) {
  wgpuQueueSubmit(wgpuQueue, 1, &cmdBuffer);
  ++numPendingCommandsSubmitted;
}

void Context::present() {
  assert(mainOutput);
  if (numPendingCommandsSubmitted > 0) {
#if GFX_EMSCRIPTEN
    auto callback = [](double time, void *userData) -> EM_BOOL {
      spdlog::debug("Animation frame callback {}", time);
      return true;
    };
    spdlog::debug("Animation frame queued");
    emscripten_request_animation_frame(callback, this);
    emscripten_sleep(0);
#else
    wgpuSwapChainPresent(mainOutput->wgpuSwapchain);
#endif
  }
  numPendingCommandsSubmitted = 0;
}

} // namespace gfx
