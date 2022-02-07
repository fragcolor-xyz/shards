#pragma once
#include "gfx_wgpu.hpp"
#include "linalg.hpp"
#include "pipeline_step.hpp"
#include <functional>
#include <memory>

namespace gfx {

struct Context;
struct Pipeline;
struct DrawQueue;

struct Drawable;
typedef std::shared_ptr<Drawable> DrawablePtr;

struct View;
typedef std::shared_ptr<View> ViewPtr;

struct Feature;
typedef std::shared_ptr<Feature> FeaturePtr;

// Instance that caches render pipelines
struct RendererImpl;
struct Renderer {
  std::shared_ptr<RendererImpl> impl;

  struct MainOutput {
    WGPUTextureView view;
    WGPUTextureFormat format;
    int2 size;
  };

public:
  Renderer(Context &context);
  void render(const DrawQueue &drawQueue, std::vector<ViewPtr> views, const PipelineSteps &pipelineSteps);
  void render(const DrawQueue &drawQueue, ViewPtr view, const PipelineSteps &pipelineSteps);
  void setMainOutput(const MainOutput &output);

  // Call before frame rendering to swap buffers
  void swapBuffers();

  // Flushes rendering and cleans up all cached data kept by the renderer
  void cleanup();
};

} // namespace gfx
