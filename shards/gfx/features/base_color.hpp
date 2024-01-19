#ifndef GFX_FEATURES_BASE_COLOR
#define GFX_FEATURES_BASE_COLOR

#include <gfx/shader/wgsl_mapping.hpp>
#include <gfx/enums.hpp>
#include <gfx/feature.hpp>
#include <gfx/params.hpp>
#include <gfx/shader/blocks.hpp>
#include <memory>

namespace gfx {
namespace features {

inline size_t getShaderFieldMaxValue(ShaderFieldBaseType baseType) {
  switch (baseType) {
  case ShaderFieldBaseType::UInt8:
    return std::numeric_limits<uint8_t>::max();
  case ShaderFieldBaseType::Int8:
    return std::numeric_limits<int8_t>::max();
  case ShaderFieldBaseType::UInt16:
    return std::numeric_limits<uint16_t>::max();
  case ShaderFieldBaseType::Int16:
    return std::numeric_limits<int16_t>::max();
  case ShaderFieldBaseType::UInt32:
    return std::numeric_limits<uint32_t>::max();
  case ShaderFieldBaseType::Int32:
    return std::numeric_limits<int32_t>::max();
  default:
    throw std::logic_error("Unexpected shader field type");
  }
}

struct BaseColor {
  static inline FeaturePtr create() {
    using namespace shader;
    using namespace shader::blocks;
    using shader::Types;
    using shader::NumType;

    NumType colorFieldType(ShaderFieldBaseType::Float32, 4);

    FeaturePtr feature = std::make_shared<Feature>();
    feature->shaderParams["baseColor"] = NumParamDecl(float4(1, 1, 1, 1));
    feature->textureParams["baseColorTexture"] = TextureParamDecl();

    feature->state.set_blend(BlendState{
        .color = BlendComponent::Alpha,
        .alpha = BlendComponent::Opaque,
    });

    const char defaultColor[] = "vec4<f32>(1.0, 1.0, 1.0, 1.0)";
    const char colorAttributeName[] = "color";

    auto initColor = makeBlock<Custom>([=](IGeneratorContext &context) {
      context.writeGlobal("color", colorFieldType, [&]() {
        auto &inputs = context.getDefinitions().inputs;
        if (context.hasInput(colorAttributeName)) {
          auto it = inputs.find(colorAttributeName);
          auto colorInputType = it != inputs.end() ? it->second : Types::Float4;

          // Convert value to color or use float type directly
          if (isFloatType(colorInputType.baseType)) {
            context.readInput("color");
          } else {
            uint64_t maxValue = getShaderFieldMaxValue(colorInputType.baseType);
            auto colorFieldTypeName =
                getWGSLTypeName(NumType(ShaderFieldBaseType::Float32, colorInputType.numComponents));
            context.write(fmt::format("({}(", colorFieldTypeName));
            context.readInput("color");
            context.write(fmt::format(") / {}(f32({:e}))", colorFieldTypeName, double(maxValue)));
            context.write(")");
          }
        } else {
          context.write(defaultColor);
        }

        context.write(" * ");
        context.readBuffer("baseColor", Types::Float4, "object");
      });
    });
    feature->shaderEntryPoints.emplace_back("initColor", ProgrammableGraphicsStage::Vertex, std::move(initColor));

    auto &writeColor = feature->shaderEntryPoints.emplace_back("writeColor", ProgrammableGraphicsStage::Vertex,
                                                               WriteOutput("color", colorFieldType, ReadGlobal("color")));
    writeColor.dependencies.emplace_back("initColor");

    // Read vertex color from vertex
    feature->shaderEntryPoints.emplace_back(
        "readColor", ProgrammableGraphicsStage::Fragment,
        WriteGlobal("color", colorFieldType, WithInput("color", ReadInput("color"), defaultColor)));

    // Apply base color texture
    auto &sampleTexture = feature->shaderEntryPoints.emplace_back(
        "textureColor", ProgrammableGraphicsStage::Fragment,
        WithTexture("baseColorTexture", true,
                    WriteGlobal("color", colorFieldType, ReadGlobal("color"), "*", SampleTexture("baseColorTexture"))));
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
