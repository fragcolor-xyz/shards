#ifndef B755A658_60B4_4264_915A_4F4571F9CBD0
#define B755A658_60B4_4264_915A_4F4571F9CBD0

#include "drawable.hpp"
#include "pipeline_builder.hpp"
#include "renderer_types.hpp"
#include "hasherxxh128.hpp"

namespace gfx {

// TODO: Move me
struct DrawData {};

struct PipelineHasher {};

struct IDrawableProcessor : public IPipelineModifier {
  virtual ~IDrawableProcessor() = default;
  virtual void buildPipeline(PipelineBuilder &builder) override = 0;
  virtual void computeDrawableHash(IDrawable &drawable, PipelineHasher &hasher) = 0;
};

}; // namespace gfx

#endif /* B755A658_60B4_4264_915A_4F4571F9CBD0 */
