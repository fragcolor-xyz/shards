#ifndef GFX_CONTEXT
#define GFX_CONTEXT

#include "enums.hpp"
#include "gfx_wgpu.hpp"
#include "../core/platform.hpp"
#include "types.hpp"
#include "fwd.hpp"
#include "user_data.hpp"
#include <cassert>
#include <list>
#include <map>
#include <memory>
#include <set>
#include <unordered_map>
#include <vector>
#include <shared_mutex>

namespace gfx {
struct MetalViewContainer;

struct ContextCreationOptions {
  bool debug = false;
  void *overrideNativeWindowHandle = nullptr;
};

enum class ContextState {
  Uninitialized,
  Requesting,
  Ok,
  Incomplete,
};

enum class ContextFrameState {
  Ok,
  WaitingForEnd,
};

struct Window;
struct ContextData;
struct ContextMainOutput;
struct DeviceRequest;
struct AdapterRequest;
namespace detail {
struct GraphicsExecutor;
}

/// <div rustbindgen opaque></div>
struct Context {
public:
  WGPUInstance wgpuInstance = nullptr;
  WGPUAdapter wgpuAdapter = nullptr;
  WGPUDevice wgpuDevice = nullptr;
  WGPUQueue wgpuQueue = nullptr;
#if WEBGPU_NATIVE
  WGPUInstanceBackendFlags instanceBackends{};
#endif

private:
  std::shared_ptr<DeviceRequest> deviceRequest;
  std::shared_ptr<AdapterRequest> adapterRequest;
  std::shared_ptr<ContextMainOutput> mainOutput;
  ContextState state = ContextState::Uninitialized;
  ContextFrameState frameState = ContextFrameState::Ok;
  bool suspended = false;

  ContextCreationOptions options;

  // TODO: Remove
  std::unordered_map<ContextData *, std::weak_ptr<ContextData>> contextDatas;
  std::shared_mutex contextDataLock;

public:
  Context();
  ~Context();

  // Initialize a context on a window's surface
  void init(Window &window, const ContextCreationOptions &options = ContextCreationOptions{});
  // Initialize headless context
  void init(const ContextCreationOptions &options = ContextCreationOptions{});

  void release();
  bool isInitialized() const { return state != ContextState::Uninitialized; }

  // Checks if the the context is ready to use
  // while requesting a device this returns false
  bool isReady() const { return state == ContextState::Ok; }

  // While isReady returns false, this ticks device/adapter initialization
  void tickRequesting();

  Window &getWindow();
  void resizeMainOutputConditional(const int2 &newSize);
  TexturePtr getMainOutputTexture();
  bool isHeadless() const;

  // Returns when a frame can be rendered
  // Returns false while device is lost an can not be rerequestd
  bool beginFrame();
  void endFrame();

  // Wait=true corresponds to wgt::Maintain::Wait in wgpu::Device::poll (https://docs.rs/wgpu/latest/wgpu/struct.Device.html)
  // which will wait for the most recently submitted command buffer
  // if wait is false it will just check for callbacks to run
  void poll(bool wait = true);

  // When entering background, releases all graphics resources and pause rendering
  void suspend();
  // Call after suspend when resuming, will recreate graphics device and continue rendering
  void resume();

  void submit(WGPUCommandBuffer cmdBuffer);

  // start tracking an object implementing WithContextData so it's data is released with this context
  void addContextDataInternal(const std::weak_ptr<ContextData> &ptr);
  void removeContextDataInternal(ContextData *ptr);

private:
  void deviceLost();

  void deviceObtained();

  void requestAdapter();
  void releaseAdapter();

  void requestDevice();
  void releaseDevice();

  WGPUSurface getOrCreateSurface();

  void initCommon();

  void present();

  void collectContextData();
  void releaseAllContextData();
};

} // namespace gfx

#endif // GFX_CONTEXT
