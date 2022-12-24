#include "context.hpp"
#include "context_data.hpp"
#include "error_utils.hpp"
#include "platform.hpp"
#include "window.hpp"
#include "log.hpp"
#include <tracy/Tracy.hpp>
#include <magic_enum.hpp>
#include <spdlog/fmt/fmt.h>
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <SDL_stdinc.h>
#include "context_xr_gfx.hpp"
//#include <openxr-integration/OpenXRSystem.h>


#if GFX_EMSCRIPTEN
#include <emscripten/html5.h>
#endif

#ifdef TRACY_ENABLE
// Defined in the gfx rust crate
//   used to initialize tracy on the rust side, since it required special intialization (C++ doesn't)
//   but since we link to the dll, we can use it from C++ too
extern "C" void gfxTracyInit();
#endif

namespace gfx {
static void gfxTracyInitOnce() {
#ifdef TRACY_ENABLE
  static bool tracyInitialized{};
  if (!tracyInitialized) {
    gfxTracyInit();
    tracyInitialized = true;
  }
#endif
}

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
  for(size_t i=0; i< mainOutput.size(); i++){
    mainOutput.at(i).reset();
  }

  WGPU_SAFE_RELEASE(wgpuSurfaceRelease, wgpuSurface);
  WGPU_SAFE_RELEASE(wgpuInstanceRelease, wgpuInstance);
}

Window &Context::getWindow() {
  assert(window);
  return *window;
}

bool Context::isHeadless() const { 
  return !mainOutput.at(0); 
}

std::vector<std::weak_ptr<IContextMainOutput>> Context::getMainOutput() const {
  std::vector<std::weak_ptr<IContextMainOutput>> mainOutputwk;
  mainOutputwk.resize(mainOutput.size());
  for(size_t i=0; i< mainOutput.size(); i++){
    mainOutputwk.at(i) = mainOutput.at(i); 
  }
  return mainOutputwk;
}

void Context::addContextDataInternal(const std::weak_ptr<ContextData> &ptr) {
  ZoneScoped;
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
    //[t] mainOutput is array because if it comes from VR it has headset, and mirror view.
    for(size_t i=0; i< mainOutput.size(); i++){
      const int maxAttempts = 2;
      std::vector<WGPUTextureView> textureViewArr;

      // Try to request the swapchain texture, automatically recreate swapchain on failure
      /*
      for (size_t i = 0; !textureViewArr && i < maxAttempts; i++) {
        textureViewArr = mainOutput.at(i)->requestFrame();
      } */
      size_t t = 0;
      bool internalsNull = true;
      do{
        textureViewArr = mainOutput.at(i)->requestFrame(); 
        internalsNull = true;
        //[t] check if all the textures exist
        for(size_t a=0; a< textureViewArr.size(); a++){
          if(!textureViewArr.at(a)){
            internalsNull = true;
            break;
          }
          else{
            internalsNull = false;
          }
        }
        t++;
      }while(internalsNull && t < maxAttempts);

      //if (!textureView)
      if(internalsNull)
        return false;
    }
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

void Context::sync() {
  ZoneScoped;
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
    SPDLOG_LOGGER_ERROR(logger, "{} ({})", message, type);
    if (type == WGPUErrorType_DeviceLost) {
      context.deviceLost();
    }
  };
  wgpuDeviceSetUncapturedErrorCallback(wgpuDevice, errorCallback, this);
  wgpuQueue = wgpuDeviceGetQueue(wgpuDevice);

  if (window) {
    mainOutput = backend->createMainOutput(getWindow());
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

  SPDLOG_LOGGER_DEBUG(logger, "Requesting wgpu device");
  deviceRequest = backend->requestDevice();
}

void Context::releaseDevice() {
  ZoneScoped;
  releaseAllContextData();

  for(size_t i=0; i< mainOutput.size(); i++){
    mainOutput.at(i).reset();
  }

  WGPU_SAFE_RELEASE(wgpuQueueRelease, wgpuQueue);
  WGPU_SAFE_RELEASE(wgpuDeviceRelease, wgpuDevice);
}

void Context::requestAdapter() {
  ZoneScoped;
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
  adapterRequest = backend->requestAdapter();
}

void Context::releaseAdapter() {
  ZoneScoped;
  releaseDevice();
  WGPU_SAFE_RELEASE(wgpuAdapterRelease, wgpuAdapter);
}

void Context::initCommon() {
  spdlog::info("[log][t] Context::initCommon()");

  gfxTracyInitOnce();

  ZoneScoped;
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

  //[t] TODO: what does each level control exactly? What is the usage of this?
  if (logger->level() <= spdlog::level::debug) {
    wgpuSetLogLevel(WGPULogLevel_Debug);
  } else {
    wgpuSetLogLevel(WGPULogLevel_Info);
  }
  #endif
  
  spdlog::info("[log][t] Context::initCommon: Creating backend: ContextXrGfxBackend.");
  //[t] Context_XR.cpp Context_XR and context_xr_gfx.cpp ContextXrGfxBackend, are both used by the headset.cpp, to create an openxr instance and openxr swapchains.

  backend = std::make_shared<ContextXrGfxBackend>();   
  wgpuInstance = backend->createInstance(); 
  
  
  spdlog::info("[log][t] Context::initCommon: Creating window/surface.");
  //[t] create mirror view with the context_xr_gfx.cpp
  //[t] and check if MirrorView was successful at CreateMirrorSurface()
  //[t] Setup surface
  if (window) { 
    assert(!wgpuSurface);
    wgpuSurface = backend->createSurface(getWindow(), options.overrideNativeWindowHandle);
  }
  
  requestDevice(); 
  //openXRSystem.createHeadset() moved into ContextXrGfxBackend's createMainOutput()
  spdlog::info("[log][t] Context::initCommon: End.");
}



void Context::present() {
  ZoneScoped;
  for(size_t i=0; i< mainOutput.size(); i++)
  {
    assert(mainOutput.at(i));
    mainOutput.at(i)->present();
  }
}

} // namespace gfx
