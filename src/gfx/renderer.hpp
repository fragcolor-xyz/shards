#ifndef GFX_RENDERER
#define GFX_RENDERER

#include "fwd.hpp"
#include "gfx_wgpu.hpp"
#include "linalg.hpp"
#include "pipeline_step.hpp"
#include "view_stack.hpp"
#include <functional>
#include <memory>

namespace gfx {

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
  void render(ViewPtr view, const PipelineSteps &pipelineSteps, bool immediate = false);

  /// <div rustbindgen hide></div>
  void setMainOutput(const MainOutput &output);

  ViewStack &getViewStack();

  /// <div rustbindgen hide></div>
  void beginFrame();

  /// <div rustbindgen hide></div>
  void endFrame();

  /// Flushes rendering and cleans up all cached data kept by the renderer
  /// <div rustbindgen hide></div>
  void cleanup();

  // Toggle whether to ignore shader and pipeline compilation errors
  void setIgnoreCompilationErrors(bool ignore);

  void dumpStats();
};

} // namespace gfx

#endif // GFX_RENDERER
