#ifndef GFX_FEATURES_BASE_COLOR
#define GFX_FEATURES_BASE_COLOR

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
    using shader::FieldType;
    using shader::FieldTypes;

    FieldType colorFieldType(ShaderFieldBaseType::Float32, 4);

    FeaturePtr feature = std::make_shared<Feature>();
    feature->shaderParams.emplace_back("baseColor", float4(1, 1, 1, 1));
    feature->textureParams.emplace_back("baseColor");

    const char *defaultColor = "vec4<f32>(1.0, 1.0, 1.0, 1.0)";

    auto readColorParam = makeCompoundBlock(ReadBuffer("baseColor", FieldTypes::Float4));

    feature->shaderEntryPoints.emplace_back("initColor", ProgrammableGraphicsStage::Vertex,
                                            WriteGlobal("color", colorFieldType,
                                                        WithInput("color", ReadInput("color"), defaultColor), "*",
                                                        std::move(readColorParam)));
    auto &writeColor = feature->shaderEntryPoints.emplace_back("writeColor", ProgrammableGraphicsStage::Vertex,
                                                               WriteOutput("color", colorFieldType, ReadGlobal("color")));
    writeColor.dependencies.emplace_back("initColor");

    // Apply vertex color
    EntryPoint &applyVertexColor = feature->shaderEntryPoints.emplace_back(
        "applyVertexColor", ProgrammableGraphicsStage::Vertex,
        WithInput("color",
                  WriteGlobal("color", colorFieldType, makeCompoundBlock(ReadGlobal("color"), "*", ReadInput("color"), ";"))));
    applyVertexColor.dependencies.emplace_back("initColor", DependencyType::After);
    applyVertexColor.dependencies.emplace_back("writeColor", DependencyType::Before);

    // Read vertex color from vertex
    feature->shaderEntryPoints.emplace_back(
        "readColor", ProgrammableGraphicsStage::Fragment,
        WriteGlobal("color", colorFieldType, WithInput("color", ReadInput("color"), defaultColor)));

    // Apply base color texture
    auto &sampleTexture = feature->shaderEntryPoints.emplace_back(
        "textureColor", ProgrammableGraphicsStage::Fragment,
        WithTexture("baseColor", WriteGlobal("color", colorFieldType, ReadGlobal("color"), "*", SampleTexture("baseColor"))));
    sampleTexture.dependencies.emplace_back("readColor");
    sampleTexture.dependencies.emplace_back("writeColor", DependencyType::Before);

    // Write output color
    auto &writeFragColor =
        feature->shaderEntryPoints.emplace_back("writeColor", ProgrammableGraphicsStage::Fragment,
                                                WithOutput("color", WriteOutput("color", colorFieldType, ReadGlobal("color"))));
    writeFragColor.dependencies.emplace_back("readColor");

    return feature;
  }
};

} // namespace features
} // namespace gfx

#endif // GFX_FEATURES_BASE_COLOR
