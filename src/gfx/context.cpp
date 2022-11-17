#include "context.hpp"
#include "context_data.hpp"
#include "error_utils.hpp"
#include "platform.hpp"
#include "window.hpp"
#include "log.hpp"
#include <magic_enum.hpp>
#include <spdlog/fmt/fmt.h>
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <SDL_stdinc.h>
#include "context_xr_gfx.hpp"

#if GFX_EMSCRIPTEN
#include <emscripten/html5.h>
#endif

namespace gfx {

Context::Context() {}
Context::~Context() { release(); }

void Context::init(Window &window, const ContextCreationOptions &inOptions) {
  options = inOptions;
  this->window = &window;

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

  WGPU_SAFE_RELEASE(wgpuSurfaceRelease, wgpuSurface);
  WGPU_SAFE_RELEASE(wgpuInstanceRelease, wgpuInstance);
}

Window &Context::getWindow() {
  assert(window);
  return *window;
}

bool Context::isHeadless() const { return !mainOutput; }

std::weak_ptr<IContextMainOutput> Context::getMainOutput() const { return mainOutput; }

void Context::addContextDataInternal(const std::weak_ptr<ContextData> &ptr) {
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
    WGPUTextureView textureView{};

    // Try to request the swapchain texture, automatically recreate swapchain on failure
    for (size_t i = 0; !textureView && i < maxAttempts; i++) {
      textureView = mainOutput->requestFrame();
    }

    if (!textureView)
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
  wgpuDevicePoll(wgpuDevice, true, nullptr);
#endif
}

void Context::suspend() {
#if GFX_ANDROID
  // Also release the surface on suspend
  deviceLost();

  mainOutput.reset();

#elif GFX_APPLE
  deviceLost();
#endif

  suspended = true;
}

void Context::resume() { suspended = false; }

void Context::submit(WGPUCommandBuffer cmdBuffer) { wgpuQueueSubmit(wgpuQueue, 1, &cmdBuffer); }

void Context::deviceLost() {
  if (state != ContextState::Incomplete) {
    SPDLOG_LOGGER_DEBUG(logger, "Device lost");
    state = ContextState::Incomplete;

    releaseDevice();
  }
}

void Context::tickRequesting() {
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
  state = ContextState::Ok;
  SPDLOG_LOGGER_DEBUG(logger, "wgpuDevice obtained");

  auto errorCallback = [](WGPUErrorType type, char const *message, void *userdata) {
    Context &context = *(Context *)userdata;
    std::string msgString(message);
    SPDLOG_LOGGER_ERROR(logger, "{} ({})", message, type);
    if (type == WGPUErrorType_DeviceLost) {
      context.deviceLost();
    }
  };
  wgpuDeviceSetUncapturedErrorCallback(wgpuDevice, errorCallback, this);
  wgpuQueue = wgpuDeviceGetQueue(wgpuDevice);

  if (window) {
    mainOutput = backendXrGfx->createMainOutput(getWindow());
  }

  WGPUDeviceLostCallback deviceLostCallback = [](WGPUDeviceLostReason reason, char const *message, void *userdata) {
    SPDLOG_LOGGER_WARN(logger, "Device lost: {} ()", message, magic_enum::enum_name(reason));
  };
  wgpuDeviceSetDeviceLostCallback(wgpuDevice, deviceLostCallback, this);
}

void Context::requestDevice() {
  assert(!wgpuDevice);
  assert(!wgpuQueue);

  // Request adapter first
  if (!wgpuAdapter) {
    requestAdapter();
    tickRequesting(); // This chains into another requestDevice call once the adapter is obtained
    return;
  }

  state = ContextState::Requesting;

  SPDLOG_LOGGER_DEBUG(logger, "Requesting wgpu device");
  deviceRequest = backendXrGfx->requestDevice();
}

void Context::releaseDevice() {
  releaseAllContextData();

  mainOutput.reset();

  WGPU_SAFE_RELEASE(wgpuQueueRelease, wgpuQueue);
  WGPU_SAFE_RELEASE(wgpuDeviceRelease, wgpuDevice);
}

void Context::requestAdapter() {
  assert(!wgpuAdapter);

  state = ContextState::Requesting;

  //   WGPURequestAdapterOptions requestAdapter = {};
  //   requestAdapter.powerPreference = WGPUPowerPreference_HighPerformance;
  //   requestAdapter.compatibleSurface = getOrCreateSurface();
  //   requestAdapter.forceFallbackAdapter = false;

  // #ifdef WEBGPU_NATIVE
  //   WGPUAdapterExtras adapterExtras = {};
  //   requestAdapter.nextInChain = &adapterExtras.chain;
  //   adapterExtras.chain.sType = (WGPUSType)WGPUSType_AdapterExtras;

  //   adapterExtras.backend = WGPUBackendType_Null;
  //   if (const char *backendStr = SDL_getenv("GFX_BACKEND")) {
  //     std::string typeStr = std::string("WGPUBackendType_") + backendStr;
  //     auto foundValue = magic_enum::enum_cast<WGPUBackendType>(typeStr);
  //     if (foundValue) {
  //       adapterExtras.backend = foundValue.value();
  //     }
  //   }

  //   if (adapterExtras.backend == WGPUBackendType_Null)
  //     adapterExtras.backend = getDefaultWgpuBackendType();

  //   SPDLOG_LOGGER_INFO(logger, "Using backend {}", magic_enum::enum_name(adapterExtras.backend));
  // #endif

  SPDLOG_LOGGER_DEBUG(logger, "Requesting wgpu adapter");
  adapterRequest = backendXrGfx->requestAdapter();
}

void Context::releaseAdapter() {
  releaseDevice();
  WGPU_SAFE_RELEASE(wgpuAdapterRelease, wgpuAdapter);
}

void Context::initCommon() {
  SPDLOG_LOGGER_DEBUG(logger, "initCommon");

  assert(!isInitialized());

#ifdef WEBGPU_NATIVE
  wgpuSetLogCallback(
      [](WGPULogLevel level, const char *msg, void *userData) {
        Context &context = *(Context *)userData;
        switch (level) {
        case WGPULogLevel_Error:
          logger->error("{}", msg);
          break;
        case WGPULogLevel_Warn:
          logger->warn("{}", msg);
          break;
        case WGPULogLevel_Info:
          logger->info("{}", msg);
          break;
        case WGPULogLevel_Debug:
          logger->debug("{}", msg);
          break;
        case WGPULogLevel_Trace:
          logger->trace("{}", msg);
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

  //[t] I'm not sure I'm liking this combination of 2 different contexts (this context, and the openxr context)
  //[t] because the opexr version is really different to this one, hard to pretend they're the same context. 
  //[t] And actually we need a 3rd Context.cpp that creates xr devices and instances.
  //[t] for example getXrInstance, getXrSystemId, getXrViewType.
  //[t] So we have this gfx context that contains the ContextXrGfxBackend which will actually be sent 
  //[t] along with Context_XR.cpp to the headset, because there's a lot more to do to create an open xr instance...
  backendXrGfx = std::make_shared<ContextXrGfxBackend>();  
  wgpuInstance = backendXrGfx->wgpuVkCreateInstance();

  // Setup surface
  if (window) { 
    assert(!wgpuSurface);
    wgpuSurface = backendXrGfx->createSurface(getWindow(), options.overrideNativeWindowHandle);
  }

  requestDevice(); 
}

//[t] called by openXRMain in OpenXRMain.cpp.
ContextXrGfxBackend Context::getContextXrGfxBackend () {
  return backendXrGfx; 
}

void Context::present() {
  assert(mainOutput);
  mainOutput->present();
}

} // namespace gfx
