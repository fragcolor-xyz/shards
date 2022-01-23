#pragma once

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

    FieldType positionFieldType(ShaderFieldBaseType::Float32, 4);
    auto vec4Pos = makeCompoundBlock("vec4<f32>(", ReadInput("position"), ".xyz, 1.0)");

    feature->shaderEntryPoints.emplace_back(
        "initWorldPosition", ProgrammableGraphicsStage::Vertex,
        WriteGlobal("worldPosition", positionFieldType, ReadBuffer("object"), ".world", "*", vec4Pos->clone()));
    auto &initScreenPosition =
        feature->shaderEntryPoints.emplace_back("initScreenPosition", ProgrammableGraphicsStage::Vertex,
                                                WriteGlobal("screenPosition", positionFieldType, ReadBuffer("view"), ".proj", "*",
                                                            ReadBuffer("view"), ".view", "*", ReadGlobal("worldPosition")));
    initScreenPosition.dependencies.emplace_back("initWorldPosition");

    auto &writePosition = feature->shaderEntryPoints.emplace_back("writePosition", ProgrammableGraphicsStage::Vertex,
                                                                  WriteOutput("position", ReadGlobal("screenPosition")));
    writePosition.dependencies.emplace_back("initScreenPosition");

    return feature;
  }
};

} // namespace features
} // namespace gfx
