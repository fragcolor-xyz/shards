#ifndef GFX_FEATURES_WIREFRAME
#define GFX_FEATURES_WIREFRAME

#include <gfx/enums.hpp>
#include <gfx/feature.hpp>
#include <gfx/params.hpp>
#include <gfx/shader/blocks.hpp>
#include <memory>

namespace gfx {
namespace features {

struct Wireframe {
  static inline FeaturePtr create(bool showBackfaces = false) {
    using namespace shader;
    using namespace shader::blocks;

    FeaturePtr feature = std::make_shared<Feature>();

    if (showBackfaces) {
      feature->state.set_depthWrite(false);
      feature->state.set_culling(false);
      feature->state.set_depthCompare(WGPUCompareFunction_Always);
    } else {
      feature->state.set_depthCompare(WGPUCompareFunction_LessEqual);
    }

    feature->state.set_blend(BlendState{
        .color = BlendComponent::AlphaPremultiplied,
        .alpha = BlendComponent::AlphaPremultiplied,
    });

    // Generate vertex barycentric coordinates from a non-indexed mesh (all unique vertices)
    auto code = makeCompoundBlock();
    code->appendLine("var bx: i32 = max(0, ((i32(", blocks::ReadInput("vertex_index"), ") + 4) % 3) - 1)");
    code->appendLine("var by: i32 = max(0, ((i32(", blocks::ReadInput("vertex_index"), ") + 2) % 3) - 1)");
    code->appendLine("var bz: i32 = 1 - bx - by");
    code->append(blocks::WriteOutput("barycentricCoord", Types::Float3, "vec3<f32>(f32(bx), f32(by), f32(bz))"));
    feature->shaderEntryPoints.emplace_back("initBarycentricCoordinates", ProgrammableGraphicsStage::Vertex, std::move(code));

    // Generate screen space edge distance to wireframe color
    code = blocks::makeCompoundBlock();
    code->appendLine("var bary = ", blocks::ReadInput("barycentricCoord"));
    code->appendLine("var deltas = fwidth(bary)");
    code->appendLine("bary = smoothstep(deltas, deltas * 1.0, bary)");
    code->appendLine("var distance = min(bary.x, min(bary.y, bary.z))");
    code->appendLine("var wire = max(0.0, 1.0 - round(distance))");
    code->append(blocks::WriteGlobal("color", Types::Float4, "wire * ", blocks::ReadBuffer("baseColor", Types::Float4)));
    code->appendLine("if(wire == 0.0) {discard;}");
    auto &entry = feature->shaderEntryPoints.emplace_back("visualizeBarycentricCoordinates", ProgrammableGraphicsStage::Fragment,
                                                          std::move(code));
    entry.dependencies.emplace_back("initColor");

    return feature;
  }
};

} // namespace features
} // namespace gfx

#endif // GFX_FEATURES_WIREFRAME
