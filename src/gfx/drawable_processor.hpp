#ifndef B755A658_60B4_4264_915A_4F4571F9CBD0
#define B755A658_60B4_4264_915A_4F4571F9CBD0

#include "drawable.hpp"
#include "pipeline_builder.hpp"
#include "renderer_types.hpp"

namespace gfx {

// TODO: Move me
struct DrawData {};

struct IDrawableProcessor : public IPipelineModifier {
  virtual ~IDrawableProcessor() = default;
  virtual void buildPipeline(PipelineBuilder &builder, const IDrawable& referenceDrawable) override = 0;
};

}; // namespace gfx

#endif /* B755A658_60B4_4264_915A_4F4571F9CBD0 */
