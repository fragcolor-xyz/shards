#ifndef B755A658_60B4_4264_915A_4F4571F9CBD0
#define B755A658_60B4_4264_915A_4F4571F9CBD0

#include "drawable.hpp"
#include "pipeline_builder.hpp"
#include "renderer_types.hpp"
#include "transient_ptr.hpp"
#include <taskflow/taskflow.hpp>
#include "pmr/wrapper.hpp"
#include <memory>

namespace gfx::detail {

struct AsyncGraphicsContext;

// Context data when evaluating rendering commands
struct DrawablePrepareContext {
  using Allocator = shards::pmr::PolymorphicAllocator<>;

  Context &context;

  WorkerMemory &workerMemory;

  const CachedPipeline &cachedPipeline;

  // View data
  const ViewData &viewData;

  // The drawables
  const std::vector<const IDrawable *> &drawables;

  // Global current frame number
  size_t frameCounter;

  // Sorting mode for drawables, set on the pipeline step
  SortMode sortMode;
};

struct DrawableEncodeContext {
  WGPURenderPassEncoder encoder;
  const CachedPipeline &cachedPipeline;
  const ViewData &viewData;

  // Data that was returned by prepare
  TransientPtr preparedData;
};

struct IDrawableProcessor : public IPipelineModifier {
  virtual ~IDrawableProcessor() = default;

  // Called at the start of a frame, before rendering
  virtual void reset(size_t frameCounter) = 0;

  // Hook for modifying the built pipeline for the referenceDrawable.
  // Whenever a group of objects is grouped under new pipeline, this function is called once before building.
  virtual void buildPipeline(PipelineBuilder &builder) override = 0;

  // Request to prepare draw data (buffers/bindings/etc.)
  // the returned value is passed to encode through DrawableEncodeContext
  virtual TransientPtr prepare(DrawablePrepareContext &context) = 0;

  // Called to encode the render commands
  virtual void encode(DrawableEncodeContext &context) = 0;
};

} // namespace gfx::detail

#endif /* B755A658_60B4_4264_915A_4F4571F9CBD0 */
