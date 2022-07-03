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
    } else {
      feature->state.set_depthCompare(WGPUCompareFunction_LessEqual);
    }

    feature->state.set_blend(BlendState{
        .color = BlendComponent::AlphaPremultiplied,
        .alpha = BlendComponent::AlphaPremultiplied,
    });

    // Generate vertex barycentric coordinates from a non-indexed mesh (all unique vertices)
    auto a = blocks::makeCompoundBlock("var bx: i32 = max(0, ((i32(", blocks::ReadInput("vertex_index"), ") + 4) % 3) - 1);\n");
    auto b = blocks::makeCompoundBlock("var by: i32 = max(0, ((i32(", blocks::ReadInput("vertex_index"), ") + 2) % 3) - 1);\n");
    auto c = blocks::makeCompoundBlock("var bz: i32 = 1 - bx - by;\n");
    auto initBarycentricCoordinates =
        makeCompoundBlock(std::move(a), std::move(b), std::move(c),
                          blocks::WriteOutput("barycentricCoord", FieldTypes::Float3, "vec3<f32>(f32(bx), f32(by), f32(bz))"));
    feature->shaderEntryPoints.emplace_back("initBarycentricCoordinates", ProgrammableGraphicsStage::Vertex,
                                            std::move(initBarycentricCoordinates));

    // Generate screen space edge distance to wireframe color
    auto wireDistance = blocks::makeCompoundBlock("var bary = ", blocks::ReadInput("barycentricCoord"), ";\n ",
                                                  "var deltas = fwidth(bary);\n"
                                                  "bary = smoothStep(deltas, deltas * 1.0, bary);\n"
                                                  "var distance = min(bary.x, min(bary.y, bary.z));\n"
                                                  "var wire = max(0.0, 1.0 - round(distance));\n");
    auto visualizeBarycentricCoordinates = makeCompoundBlock(
        std::move(wireDistance), blocks::WriteGlobal("color", FieldTypes::Float4, "wire * vec4<f32>(1.0, 1.0, 1.0, 1.0)"));
    auto &entry = feature->shaderEntryPoints.emplace_back("visualizeBarycentricCoordinates", ProgrammableGraphicsStage::Fragment,
                                                          std::move(visualizeBarycentricCoordinates));
    entry.dependencies.emplace_back("initColor");

    return feature;
  }
};

} // namespace features
} // namespace gfx

#endif // GFX_FEATURES_WIREFRAME
