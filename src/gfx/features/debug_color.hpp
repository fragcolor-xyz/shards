#ifndef GFX_FEATURES_DEBUG_COLOR
#define GFX_FEATURES_DEBUG_COLOR

#include <gfx/enums.hpp>
#include <gfx/feature.hpp>
#include <gfx/params.hpp>
#include <gfx/shader/shards.hpp>
#include <memory>

namespace gfx {
namespace features {

struct DebugColor {
  enum class Stage { Vertex, Fragment };
  static inline FeaturePtr create(const char *fieldName, gfx::ProgrammableGraphicsStage stage) {
    using namespace shader;
    using namespace shader::shards;

    FieldType colorFieldType(ShaderFieldBaseType::Float32, 4);

    FeaturePtr feature = std::make_shared<Feature>();

    auto writeColor = [fieldName = std::string(fieldName)](GeneratorContext &context) {
      context.write("vec4<f32>(");
      auto &input = context.inputs[fieldName];
      context.readInput(fieldName.c_str());
      switch (input->type.numComponents) {
      case 1:
        context.write(", 0.0, 0.0, 1.0");
        break;
      case 2:
        context.write(".xy, 0.0, 1.0");
        break;
      case 3:
        context.write(".xyz, 1.0");
        break;
      case 4:
        context.write(".xyzw");
        break;
      }
      context.write(")");
    };
    auto &debugColor = feature->shaderEntryPoints.emplace_back(
        "debugColor", stage, WithInput(fieldName, WriteGlobal("color", colorFieldType, Custom(writeColor))));
    if (stage == gfx::ProgrammableGraphicsStage::Vertex) {
      debugColor.dependencies.emplace_back("initColor");
    } else {
      debugColor.dependencies.emplace_back("readColor");
    }
    debugColor.dependencies.emplace_back("writeColor", DependencyType::Before);

    return feature;
  }
};

} // namespace features
} // namespace gfx

#endif // GFX_FEATURES_DEBUG_COLOR
