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
  static inline FeaturePtr create(bool applyView = true, bool applyProjection = true) {
    using namespace shader;
    using namespace shader::blocks;

    FeaturePtr feature = std::make_shared<Feature>();

    auto vec4Pos = std::make_unique<blocks::Custom>([&](shader::IGeneratorContext &ctx) {
      ctx.write("vec4<f32>(");
      ctx.readInput("position");

      auto &stageInputs = ctx.getInputs();
      auto it = stageInputs.find("position");
      if (it != stageInputs.end()) {
        auto &type = it->second;
        switch (type.numComponents) {
        case 2:
          ctx.write(".xy, 0.0, 1.0)");
          break;
        case 3:
          ctx.write(".xyz, 1.0)");
          break;
        default:
          ctx.pushError(formatError("Unsupported position type"));
          break;
        }
      }
    });

    feature->shaderEntryPoints.emplace_back(
        "initLocalPosition", ProgrammableGraphicsStage::Vertex, WriteGlobal("localPosition", FieldTypes::Float4, std::move(vec4Pos)));

    auto &entry = feature->shaderEntryPoints.emplace_back("initWorldPosition", ProgrammableGraphicsStage::Vertex,
                                                          WriteGlobal("worldPosition", FieldTypes::Float4,
                                                                      ReadBuffer("world", FieldTypes::Float4x4), "*",
                                                                      ReadGlobal("localPosition")));
    entry.dependencies.emplace_back("initLocalPosition");

    auto screenPosition = makeCompoundBlock();
    if (applyProjection) {
      screenPosition->append(ReadBuffer("proj", FieldTypes::Float4x4, "view"), "*");
    }
    if (applyView) {
      screenPosition->append(ReadBuffer("view", FieldTypes::Float4x4, "view"), "*");
    }
    screenPosition->append(ReadGlobal("worldPosition"));

    auto &initScreenPosition =
        feature->shaderEntryPoints.emplace_back("initScreenPosition", ProgrammableGraphicsStage::Vertex,
                                                WriteGlobal("screenPosition", FieldTypes::Float4, std::move(screenPosition)));

    initScreenPosition.dependencies.emplace_back("initWorldPosition");

    BlockPtr transformNormal = blocks::makeCompoundBlock("normalize((", ReadBuffer("invTransWorld", FieldTypes::Float4x4), "*",
                                                         "vec4<f32>(", ReadInput("normal"), ".xyz, 0.0)", ").xyz)");
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
