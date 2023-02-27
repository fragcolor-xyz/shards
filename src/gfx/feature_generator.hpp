
#include "drawable.hpp"
#include "fwd.hpp"
#include "params.hpp"
#include "pipeline_step.hpp"
#include "renderer.hpp"
// #include "pmr/vector.hpp"
#include <span>
#include <vector>

namespace gfx::detail {
struct CachedDrawable;
struct CachedView;
} // namespace gfx::detail

namespace gfx {

struct FeatureGeneratorContext {
  Renderer &renderer;

  // The list of features excluding the one that this generator belongs to
  std::span<FeatureWeakPtr> features;

  // The view used within the parent render step
  ViewPtr view;

  // Internal view data
  const detail::CachedView &cachedView;

  // The queue used within the parent render step
  DrawQueuePtr queue;

  size_t frameCounter;

  FeatureGeneratorContext(Renderer &renderer, ViewPtr view, const detail::CachedView &cachedView, DrawQueuePtr queue,
                          size_t frameCounter)
      : renderer(renderer), view(view), cachedView(cachedView), queue(queue), frameCounter(frameCounter) {}

  // Issues a render command that will run before the parent render step
  virtual void render(std::vector<ViewPtr> views, const PipelineSteps &pipelineSteps) = 0;
};

struct FeatureViewGeneratorContext : public FeatureGeneratorContext {
  using FeatureGeneratorContext::FeatureGeneratorContext;

  // Get the parameter collector for the current view
  virtual IParameterCollector &getParameterCollector() = 0;
};

struct FeatureDrawableGeneratorContext : public FeatureGeneratorContext {
  using FeatureGeneratorContext::FeatureGeneratorContext;

  // Returns the number of drawables affected
  virtual size_t getSize() const = 0;

  // Get the parameter collector for the given drawable
  virtual IParameterCollector &getParameterCollector(size_t index) = 0;

  virtual const IDrawable &getDrawable(size_t index) = 0;
};

struct FeatureGenerator {
  using PerView = std::function<void(FeatureViewGeneratorContext &)>;
  using PerObject = std::function<void(FeatureDrawableGeneratorContext &drawable)>;

  FeatureGenerator(PerView cb) : callback(cb) {}
  FeatureGenerator(PerObject cb) : callback(cb) {}

  std::variant<PerView, PerObject> callback;
};
} // namespace gfx
