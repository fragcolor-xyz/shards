#ifndef BCB0A1D3_714A_4808_BED6_7E1D681149D2
#define BCB0A1D3_714A_4808_BED6_7E1D681149D2

#include "context.hpp"
#include "platform_surface.hpp"
#include "log.hpp"
#include <spdlog/fmt/fmt.h>
#include <spdlog/spdlog.h>
#include <gfx_rust.h>

namespace gfx {
static auto logger = gfx::getLogger();
static auto wgpuLogger = getWgpuLogger();

inline WGPUTextureFormat getDefaultSrgbBackbufferFormat() {
#if GFX_ANDROID
  return WGPUTextureFormat_RGBA8UnormSrgb;
#else
  return WGPUTextureFormat_BGRA8UnormSrgb;
#endif
}

#ifdef WEBGPU_NATIVE
inline WGPUBackendType getDefaultWgpuBackendType() {
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
  typedef std::function<void(WGPURequestAdapterStatus, WGPUAdapter, char const *)> Callback;
  typedef std::vector<Callback> Callbacks;

  WGPURequestAdapterStatus status;
  WGPUAdapter adapter;
  bool finished{};
  std::string message;
  std::vector<std::function<void(WGPURequestAdapterStatus, WGPUAdapter, char const *)>> callbacks;

  static std::shared_ptr<Self> create(WGPUInstance wgpuInstance, const WGPURequestAdapterOptions &options,
                                      Callbacks &&callbacks = Callbacks()) {
    auto result = std::make_shared<Self>();
    result->callbacks = std::move(callbacks);
    wgpuInstanceRequestAdapterEx(wgpuInstance, &options, (WGPURequestAdapterCallback)&Self::callback,
                                 new std::shared_ptr<Self>(result));
    return result;
  };

  static void callback(WGPURequestAdapterStatus status, WGPUAdapter adapter, char const *message, std::shared_ptr<Self> *handle) {
    (*handle)->status = status;
    (*handle)->adapter = adapter;
    if (message)
      (*handle)->message = message;
    (*handle)->finished = true;

    for (auto &cb : (*handle)->callbacks) {
      cb(status, adapter, message);
    }
    delete handle;
  };
};

struct DeviceRequest {
  typedef DeviceRequest Self;
  typedef std::function<void(WGPURequestDeviceStatus, WGPUDevice, char const *)> Callback;
  typedef std::vector<Callback> Callbacks;

  WGPURequestDeviceStatus status;
  WGPUDevice device;
  bool finished{};
  std::string message;
  Callbacks callbacks;

  static std::shared_ptr<Self> create(WGPUAdapter wgpuAdapter, const WGPUDeviceDescriptor &deviceDesc,
                                      Callbacks &&callbacks = Callbacks()) {
    auto result = std::make_shared<Self>();
    result->callbacks = std::move(callbacks);
    wgpuAdapterRequestDeviceEx(wgpuAdapter, &deviceDesc, (WGPURequestDeviceCallback)&Self::callback,
                               new std::shared_ptr<Self>(result));
    return result;
  };

  static void callback(WGPURequestDeviceStatus status, WGPUDevice device, char const *message, std::shared_ptr<Self> *handle) {
    (*handle)->status = status;
    (*handle)->device = device;
    if (message)
      (*handle)->message = message;
    (*handle)->finished = true;

    for (auto &cb : (*handle)->callbacks) {
      cb(status, device, message);
    }
    delete handle;
  };
};

// OpenXR prototype instance/adapter/device abstraction
struct IContextBackend {
  virtual ~IContextBackend() = default;
  virtual WGPUInstance createInstance() = 0;

  virtual WGPUSurface createSurface(Window &window, void *overrideNativeWindowHandle) = 0;

  // Requires a prior call to createSurface
  virtual std::shared_ptr<AdapterRequest> requestAdapter() = 0;
  virtual std::shared_ptr<DeviceRequest> requestDevice() = 0;

  // Requires a prior call to createSurface
  virtual std::shared_ptr<IContextMainOutput> createMainOutput(Window &window) = 0;
};

} // namespace gfx

#endif /* BCB0A1D3_714A_4808_BED6_7E1D681149D2 */
