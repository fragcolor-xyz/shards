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
struct IContextBackend;

struct IContextMainOutput {
  virtual ~IContextMainOutput() = default;

  // Is this output resizable
  virtual bool isResizable() const = 0;
  // Current output image size
  virtual const int2 &getSize() const = 0;
  // Return the texture format of the images
  virtual WGPUTextureFormat getFormat() const = 0;
  // Resize the output
  virtual void resize(const int2 &newSize) = 0;
  // Requests a new swapchain image to render to
  virtual WGPUTextureView requestFrame() = 0;
  // Returns the currently request frame's texture view
  virtual WGPUTextureView getCurrentFrame() const = 0;
  // Return the previously requested swapchain image to the chain and allow it to be displayed
  virtual void present() = 0;
};

/// <div rustbindgen opaque></div>
struct Context {
private:
  // Temporary async requests
  std::shared_ptr<DeviceRequest> deviceRequest;
  std::shared_ptr<AdapterRequest> adapterRequest;

  std::shared_ptr<IContextMainOutput> mainOutput;
  std::shared_ptr<IContextBackend> backend;

  ContextState state = ContextState::Uninitialized;
  ContextFrameState frameState = ContextFrameState::Ok;
  bool suspended = false;

public:
  Window *window{};

  WGPUInstance wgpuInstance{};
  WGPUAdapter wgpuAdapter{};
  WGPUDevice wgpuDevice{};
  WGPUQueue wgpuQueue{};
  WGPUSurface wgpuSurface{};
  ContextCreationOptions options;

  std::unordered_map<ContextData *, std::weak_ptr<ContextData>> contextDatas;
  TypedUserData userData;

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
  bool isReady() const { return state != ContextState::Requesting; }

  // While isReady returns false, this ticks device/adapter initialization
  void tickRequesting();

  Window &getWindow();

  // true if this context doesn't have a main output
  bool isHeadless() const;

  // The main output, only valid if isHeadless() == false
  std::weak_ptr<IContextMainOutput> getMainOutput() const;

  // Returns when a frame can be rendered
  // Returns false while device is lost an can not be rerequestd
  bool beginFrame();
  void endFrame();
  void sync();

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
