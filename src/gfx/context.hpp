#pragma once

#include "enums.hpp"
#include "gfx_wgpu.hpp"
#include "types.hpp"
#include <cassert>
#include <list>
#include <map>
#include <memory>
#include <set>

namespace gfx {
struct ContextCreationOptions {
  bool debug = true;
  void *overrideNativeWindowHandle = nullptr;
};

struct CopyBuffer {
  std::vector<uint8_t> data;
};

struct Context;
struct ErrorScope {
  typedef std::function<void(WGPUErrorType type, char const *message)> Function;

  bool processed = false;
  Function function;
  Context *context;

  ErrorScope(Context *context) : context(context){};
  void push(WGPUErrorFilter filter);
  void pop(ErrorScope::Function &&function);
  static void staticCallback(WGPUErrorType type, char const *message, void *userData);
};

struct ImguiContext;
struct Primitive;
struct MaterialBuilderContext;
struct FrameRenderer;
struct Window;
struct ContextImpl;
struct WithContextData;
struct Context {
private:
  std::shared_ptr<ContextImpl> impl;
  FrameRenderer *currentFrameRenderer = nullptr;
  Window *window;
  int2 mainOutputSize;
  bool initialized = false;

public:
  WGPUInstance wgpuInstance = nullptr;
  WGPUSurface wgpuSurface = nullptr;
  WGPUAdapter wgpuAdapter = nullptr;
  WGPUDevice wgpuDevice = nullptr;
  WGPUQueue wgpuQueue = nullptr;
  WGPUSwapChain wgpuSwapchain = nullptr;
  WGPUTextureFormat swapchainFormat;

  std::vector<std::shared_ptr<ErrorScope>> errorScopes;
  std::vector<std::shared_ptr<CopyBuffer>> copyBuffers;
  std::unordered_map<WithContextData *, std::weak_ptr<WithContextData>> contextDataObjects;

public:
  Context();
  ~Context();

  void init(Window &window, const ContextCreationOptions &options = ContextCreationOptions{});
  void cleanup();
  bool isInitialized() const { return initialized; }

  Window &getWindow() {
    assert(window);
    return *window;
  }

  int2 getMainOutputSize() const { return mainOutputSize; }
  void resizeMainOutput(const int2 &newSize);
  void resizeMainOutputConditional(const int2 &newSize);
  void cleanupSwapchain();

  // pushes state attached to a popErrorScope callback
  ErrorScope &pushErrorScope(WGPUErrorFilter filter = WGPUErrorFilter::WGPUErrorFilter_Validation);

  // pushes a buffer used for CPU->GPU copy
  std::shared_ptr<CopyBuffer> pushCopyBuffer();

  void beginFrame(FrameRenderer *frameRenderer);
  void endFrame(FrameRenderer *frameRenderer);
  void sync();
  FrameRenderer *getFrameRenderer() { return currentFrameRenderer; }

  // start tracking an object implementing WithContextData so it's data is released with this context
  void addContextDataObjectInternal(std::weak_ptr<WithContextData> ptr);
  void removeContextDataObjectInternal(WithContextData *ptr);

private:
  void collectContextDataObjects();
  void releaseAllContextDataObjects();
};

} // namespace gfx
