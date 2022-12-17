#ifndef B755A658_60B4_4264_915A_4F4571F9CBD0
#define B755A658_60B4_4264_915A_4F4571F9CBD0

#include "drawable.hpp"
#include "pipeline_builder.hpp"
#include "renderer_types.hpp"
#include "transient_ptr.hpp"
#include <taskflow/taskflow.hpp>
#include <memory_resource>
#include <memory>

namespace gfx::detail {

struct AsyncGraphicsContext;

namespace pmr {
using namespace std::pmr;
}

// Context data when evaluating rendering commands
struct DrawablePrepareContext {
  using Allocator = pmr::polymorphic_allocator<>;

  Context &context;

  WorkerMemories& workerMemory;

  const CachedPipeline &cachedPipeline;

  // The flow that this request runs in
  tf::Subflow &subflow;

  // View data
  const ViewData &viewData;

  // The drawables
  const std::vector<const IDrawable *> &drawables;

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
  // Whenver a group of objects is grouped under new pipeline, this function is called once before building.
  virtual void buildPipeline(PipelineBuilder &builder) override = 0;

  // Request to prepare draw data (buffers/bindings/etc.)
  // the returned value is passed to encode
  virtual TransientPtr prepare(DrawablePrepareContext &context) = 0;

  // Called to encode the render commands
  virtual void encode(DrawableEncodeContext &context) = 0;
};

} // namespace gfx::detail

#endif /* B755A658_60B4_4264_915A_4F4571F9CBD0 */
