#ifndef FE9A8471_61AF_483B_8D2F_84E570864FB2
#define FE9A8471_61AF_483B_8D2F_84E570864FB2

#include "drawables/mesh_drawable.hpp"
#include "../drawable_processor.hpp"

namespace gfx {
struct MeshDrawableProcessor final : public IDrawableProcessor {
  void buildPipeline(PipelineBuilder &builder) override {}
  void computeDrawableHash(IDrawable &_drawable, PipelineHasher &hasher) override {
    MeshDrawable &drawable = static_cast<MeshDrawable &>(_drawable);
  }

  static inline MeshDrawableProcessor &getInstance() {
    static MeshDrawableProcessor inst;
    return inst;
  }
};
} // namespace gfx

#endif /* FE9A8471_61AF_483B_8D2F_84E570864FB2 */
