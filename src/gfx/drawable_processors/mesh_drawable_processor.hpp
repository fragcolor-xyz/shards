#ifndef FE9A8471_61AF_483B_8D2F_84E570864FB2
#define FE9A8471_61AF_483B_8D2F_84E570864FB2

#include "drawables/mesh_drawable.hpp"
#include "../drawable_processor.hpp"
#include "../renderer_types.hpp"
#include "../shader/types.hpp"

namespace gfx {
struct MeshDrawableProcessor final : public IDrawableProcessor {
  void buildPipeline(PipelineBuilder &builder, const IDrawable &referenceDrawable) override {
    using namespace shader;

    auto &viewBinding = builder.getOrCreateBufferBinding("view");
    auto &viewLayoutBuilder = viewBinding.layoutBuilder;
    viewLayoutBuilder.push("view", FieldTypes::Float4x4);
    viewLayoutBuilder.push("proj", FieldTypes::Float4x4);
    viewLayoutBuilder.push("invView", FieldTypes::Float4x4);
    viewLayoutBuilder.push("invProj", FieldTypes::Float4x4);
    viewLayoutBuilder.push("viewport", FieldTypes::Float4);

    auto &objectBinding = builder.getOrCreateBufferBinding("object");
    auto &objectLayoutBuilder = objectBinding.layoutBuilder;
    objectLayoutBuilder.push("world", FieldTypes::Float4x4);
    objectLayoutBuilder.push("invWorld", FieldTypes::Float4x4);
    objectLayoutBuilder.push("invTransWorld", FieldTypes::Float4x4);

    const MeshDrawable &meshDrawable = static_cast<const MeshDrawable &>(referenceDrawable);

    // TODO: Move meshFormat to builder
    builder.cachedPipeline.meshFormat = meshDrawable.mesh->getFormat();
    for (auto &feature : meshDrawable.features) {
      builder.features.push_back(feature.get());
    }
  }

  static inline MeshDrawableProcessor &getInstance() {
    static MeshDrawableProcessor inst;
    return inst;
  }
};
} // namespace gfx

#endif /* FE9A8471_61AF_483B_8D2F_84E570864FB2 */
