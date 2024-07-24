#ifndef B755A658_60B4_4264_915A_4F4571F9CBD0
#define B755A658_60B4_4264_915A_4F4571F9CBD0

#include "drawable.hpp"
#include "pipeline_builder.hpp"
#include "renderer_types.hpp"
#include "transient_ptr.hpp"
#include <taskflow/taskflow.hpp>
#include <shards/core/pmr/wrapper.hpp>
#include <shards/core/pmr/vector.hpp>
#include <memory>

namespace gfx::detail {

struct AsyncGraphicsContext;

struct RendererStorage;

// Context data when evaluating rendering commands
struct DrawablePrepareContext {
  using Allocator = shards::pmr::PolymorphicAllocator<>;

  Context &context;

  RendererStorage &storage;

  const CachedPipeline &cachedPipeline;

  // View data
  const ViewData &viewData;

  Rect viewport;

  // The drawables
  const shards::pmr::vector<const IDrawable *> &drawables;

  // Data from feature view/drawable generators
  const GeneratorData &generatorData;

  // Sorting mode for drawables, set on the pipeline step
  SortMode sortMode;
};

struct DrawableEncodeContext {
  RendererStorage &storage;

  WGPURenderPassEncoder encoder;
  const CachedPipeline &cachedPipeline;
  const ViewData &viewData;

  Rect viewport;

  // Data that was returned by prepare
  TransientPtr preparedData;
};

struct DrawablePreprocessContext {
  const shards::pmr::vector<const IDrawable *>& drawables;
  const shards::pmr::vector<Feature *> &features;
  const BuildPipelineOptions &buildPipelineOptions;
  RendererStorage &storage;
  Hash128 sharedHash;
  Hash128 *outHashes;
};

struct IDrawableProcessor : public IPipelineModifier {
  virtual ~IDrawableProcessor() = default;

  // Called at the start of a frame, before rendering
  virtual void reset(size_t frameCounter) = 0;

  // Hook for modifying the built pipeline for the referenceDrawable.
  // Whenever a group of objects is grouped under new pipeline, this function is called once before building.
  virtual void buildPipeline(PipelineBuilder &builder, const BuildPipelineOptions &options) override = 0;

  // Request to prepare draw data (buffers/bindings/etc.)
  // the returned value is passed to encode through DrawableEncodeContext
  virtual TransientPtr prepare(DrawablePrepareContext &context) = 0;

  // Called to encode the render commands
  virtual void encode(DrawableEncodeContext &context) = 0;

  // Precompute and hashing step
  // The returned hash identifies the pipeline permutation
  virtual void preprocess(const DrawablePreprocessContext &context) = 0;
};

} // namespace gfx::detail

#endif /* B755A658_60B4_4264_915A_4F4571F9CBD0 */
