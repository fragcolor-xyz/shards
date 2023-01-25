
#include "fwd.hpp"
#include "pipeline_step.hpp"
#include "renderer.hpp"
#include <vector>
namespace gfx {

struct IFeatureGenerateContext {
  virtual Renderer& getRenderer() = 0;
  // Returns the queue used within the parent render step
  virtual DrawQueuePtr getQueue() = 0;
  // Returns the view used within the parent render step
  virtual ViewPtr getView() = 0;
  // Returns the parameter collector for parameters to add to the parent render step
  virtual IParameterCollector& getParameterCollector() = 0;
  // Issues a render command that will run before the parent render step
  virtual void render(std::vector<ViewPtr> views, const PipelineSteps &pipelineSteps) = 0;
};
} // namespace gfx
