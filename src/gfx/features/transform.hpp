#ifndef GFX_FEATURES_TRANSFORM
#define GFX_FEATURES_TRANSFORM

#include <gfx/enums.hpp>
#include <gfx/feature.hpp>
#include <gfx/params.hpp>
#include <gfx/shader/blocks.hpp>
#include <memory>

namespace gfx {
namespace features {

struct Transform {
  static inline FeaturePtr create() {
    using namespace shader;
    using namespace shader::blocks;

    FeaturePtr feature = std::make_shared<Feature>();

    auto vec4Pos = makeCompoundBlock("vec4<f32>(", ReadInput("position"), ".xyz, 1.0)");

    feature->shaderEntryPoints.emplace_back(
        "initWorldPosition", ProgrammableGraphicsStage::Vertex,
        WriteGlobal("worldPosition", FieldTypes::Float4, ReadBuffer("world", FieldTypes::Float4x4), "*", vec4Pos->clone()));
    auto &initScreenPosition = feature->shaderEntryPoints.emplace_back(
        "initScreenPosition", ProgrammableGraphicsStage::Vertex,
        WriteGlobal("screenPosition", FieldTypes::Float4, ReadBuffer("proj", FieldTypes::Float4x4, "view"), "*",
                    ReadBuffer("view", FieldTypes::Float4x4, "view"), "*", ReadGlobal("worldPosition")));
    initScreenPosition.dependencies.emplace_back("initWorldPosition");

    BlockPtr transformNormal = blocks::makeCompoundBlock("(", ReadBuffer("worldViewInvTrans", FieldTypes::Float4x4), "*",
                                                         "vec4<f32>(", ReadInput("normal"), ".xyz, 1.0)", ").xyz");
    feature->shaderEntryPoints.emplace_back(
        "initWorldNormal", ProgrammableGraphicsStage::Vertex,
        WriteGlobal("worldNormal", FieldTypes::Float3,
                    WithInput("normal", std::move(transformNormal), blocks::Direct("vec3<f32>(0.0, 0.0, 1.0)"))));

    auto &writePosition =
        feature->shaderEntryPoints.emplace_back("writePosition", ProgrammableGraphicsStage::Vertex,
                                                WriteOutput("position", FieldTypes::Float4, ReadGlobal("screenPosition")));
    writePosition.dependencies.emplace_back("initScreenPosition");

    auto &writeNormal =
        feature->shaderEntryPoints.emplace_back("writeNormal", ProgrammableGraphicsStage::Vertex,
                                                WriteOutput("worldNormal", FieldTypes::Float3, ReadGlobal("worldNormal")));
    writeNormal.dependencies.emplace_back("initWorldNormal");

    return feature;
  }
};

} // namespace features
} // namespace gfx

#endif // GFX_FEATURES_TRANSFORM
