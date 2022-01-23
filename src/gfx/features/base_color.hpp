#pragma once

#include <gfx/enums.hpp>
#include <gfx/feature.hpp>
#include <gfx/params.hpp>
#include <gfx/shader/blocks.hpp>
#include <memory>

namespace gfx {
namespace features {

struct BaseColor {
  static inline FeaturePtr create() {
    using namespace shader;
    using namespace shader::blocks;

    FieldType colorFieldType(ShaderFieldBaseType::Float32, 4);

    FeaturePtr feature = std::make_shared<Feature>();
    feature->shaderParams.emplace_back("baseColor", float4(1, 1, 1, 1));

    feature->shaderEntryPoints.emplace_back(
        "initColor", ProgrammableGraphicsStage::Vertex,
        WriteGlobal("color", colorFieldType, WithInput("color", ReadInput("color"), "vec4<f32>(1.0, 0.0, 1.0, 1.0)")));
    auto &writeColor = feature->shaderEntryPoints.emplace_back("writeColor", ProgrammableGraphicsStage::Vertex,
                                                               WriteOutput("color", ReadGlobal("color")));
    writeColor.dependencies.emplace_back("initColor");

    auto &normalColor = feature->shaderEntryPoints.emplace_back(
        "normalColor", ProgrammableGraphicsStage::Vertex,
        WithInput("normal", WriteGlobal("color", colorFieldType, "vec4<f32>(", ReadInput("normal"), ".xyz, 1.0)")));
    normalColor.dependencies.emplace_back("initColor");
    normalColor.dependencies.emplace_back("writeColor", DependencyType::Before);

    EntryPoint &applyVertexColor = feature->shaderEntryPoints.emplace_back(
        "applyVertexColor", ProgrammableGraphicsStage::Vertex,
        WithInput("color",
                  WriteGlobal("color", colorFieldType, makeCompoundBlock(ReadGlobal("color"), "*", ReadInput("color"), ";"))));
    applyVertexColor.dependencies.emplace_back("initColor", DependencyType::After);
    applyVertexColor.dependencies.emplace_back("writeColor", DependencyType::Before);

    feature->shaderEntryPoints.emplace_back("color", ProgrammableGraphicsStage::Fragment,
                                            WithInput("color", WriteOutput("color", ReadInput("color"))));

    return feature;
  }
};

} // namespace features
} // namespace gfx
