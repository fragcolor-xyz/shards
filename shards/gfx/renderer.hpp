#ifndef GFX_RENDERER
#define GFX_RENDERER

#include "fwd.hpp"
#include "gfx_wgpu.hpp"
#include "linalg.hpp"
#include "pipeline_step.hpp"
#include "view_stack.hpp"
#include "gpu_read_buffer.hpp"
#include <functional>
#include <memory>
#include <span>

namespace gfx {

struct ShapeRenderer;
struct RendererImpl;

/// Instance that caches render pipelines
/// <div rustbindgen opaque></div>
struct Renderer {
  std::shared_ptr<RendererImpl> impl;

  /// <div rustbindgen hide></div>
  struct MainOutput {
    TexturePtr texture;
  };

public:
  /// <div rustbindgen hide></div>
  Renderer(Context &context);

  Context &getContext();

  /// <div rustbindgen hide></div>
  void render(ViewPtr view, const PipelineSteps &pipelineSteps);

  void dispatch(std::span<NumParameter> params) {}

  // Queues a copy of texture data from gpu to cpu memory
  // it will be guaranteed to be written when starting the next frame
  // When setting wait=true, this will block until it has actually been read
  void copyTexture(TextureSubResource texture, GpuTextureReadBufferPtr destination, bool wait = false);

  // Queues a copy of buffer data from gpu to cpu memory
  // it will be guaranteed to be written when starting the next frame
  // When setting wait=true, this will block until it has actually been read
  void copyBuffer(BufferPtr buffer, GpuReadBufferPtr destination, bool wait = false);

  // poll pending  texture copy commands
  void pollBufferCopies();

  /// <div rustbindgen hide></div>
  void setMainOutput(const MainOutput &output);

  /// <div rustbindgen hide></div>
  const ViewStack &getViewStack() const;

  /// <div rustbindgen hide></div>
  void pushView(ViewStack::Item &&item);

  /// <div rustbindgen hide></div>
  void popView();

  /// <div rustbindgen hide></div>
  void beginFrame();

  /// <div rustbindgen hide></div>
  void endFrame();

  /// Flushes rendering and cleans up all cached data kept by the renderer
  /// <div rustbindgen hide></div>
  void cleanup();

  // Toggle whether to ignore shader and pipeline compilation errors
  void setIgnoreCompilationErrors(bool ignore);

  // Enable debug visualization mode
  void setDebug(bool debug);
  // Process visuals output by debug mode
  void processDebugVisuals(ShapeRenderer &sr);

  void dumpStats();
};

} // namespace gfx

#endif // GFX_RENDERER
