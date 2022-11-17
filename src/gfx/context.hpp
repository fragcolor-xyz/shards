#ifndef GFX_CONTEXT
#define GFX_CONTEXT

#include "enums.hpp"
#include "gfx_wgpu.hpp"
#include "platform.hpp"
#include "types.hpp"
#include "user_data.hpp"
#include <cassert>
#include <list>
#include <map>
#include <memory>
#include <set>
#include <unordered_map>
#include <vector>
#include <shared_mutex>
#include <optional>

namespace gfx {
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
struct DeviceRequest;
struct AdapterRequest;
namespace detail {
struct GraphicsExecutor;
}
struct IContextBackend;

struct IContextCurrentFramePayload {
  WGPUTextureView wgpuTextureView;
  std::optional<bool> useMatrix = false;
  std::optional<linalg::aliases::float4x4> eyeViewMatrix;
  std::optional<linalg::aliases::float4x4> eyeProjectionMatrix;
};

struct IContextMainOutput {
  virtual ~IContextMainOutput() = default;

  // Current output image size
  virtual const int2 &getSize() const = 0;
  // Return the texture format of the images
  virtual WGPUTextureFormat getFormat() const = 0;
  // Requests a new swapchain image to render to. Is an array because can contain e.g. multiple xr eyes
  virtual std::vector<WGPUTextureView> requestFrame() = 0;
  // Returns the currently request frame's texture view
  virtual std::vector<IContextCurrentFramePayload> getCurrentFrame() const = 0;
  // Return the previously requested swapchain image to the chain and allow it to be displayed
  virtual void present() = 0;
};

/// <div rustbindgen opaque></div>
struct Context {
public:
  // tmp
  WGPUSurface wgpuSurface{};

private:
  // Temporary async requests
  std::shared_ptr<DeviceRequest> deviceRequest;
  std::shared_ptr<AdapterRequest> adapterRequest;

  //[t] mainOutput is array because if it comes from VR it has headset, and mirror view.
  std::vector<std::shared_ptr<IContextMainOutput>> mainOutput;
  std::shared_ptr<IContextBackend> backend;

  ContextState state = ContextState::Uninitialized;
  ContextFrameState frameState = ContextFrameState::Ok;
  bool suspended = false;

  ContextCreationOptions options;

  // TODO: Remove
  std::unordered_map<ContextData *, std::weak_ptr<ContextData>> contextDatas;
  std::shared_mutex contextDataLock;

  Window *window{};

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

  // true if this context doesn't have a main output
  bool isHeadless() const;

  // The main output, only valid if isHeadless() == false
  std::vector<std::weak_ptr<IContextMainOutput>> getMainOutput() const;

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

  void initCommon();

  void present();

  void collectContextData();
  void releaseAllContextData();
};

} // namespace gfx

#endif // GFX_CONTEXT
