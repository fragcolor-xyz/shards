#include "transform.hpp"
#include <gfx/enums.hpp>
#include <gfx/feature.hpp>
#include <gfx/params.hpp>
#include <gfx/shader/blocks.hpp>
#include <memory>

namespace gfx {
namespace features {

static inline std::string transformLibWgsl = R"(
fn transform_qconj(q: vec4<f32>) -> vec4<f32> {
  return vec4<f32>(-q.x,-q.y,-q.z,q.w);
}
fn transform_qmul(a: vec4<f32>, b: vec4<f32>) -> vec4<f32> {
  return vec4<f32>(
    a.x*b.w+a.w*b.x+a.y*b.z-a.z*b.y,
    a.y*b.w+a.w*b.y+a.z*b.x-a.x*b.z,
    a.z*b.w+a.w*b.z+a.x*b.y-a.y*b.x,
    a.w*b.w-a.x*b.x-a.y*b.y-a.z*b.z);
}
// fn transform_qrot(q: vec4<f32>, v: vec3<f32>) -> vec3<f32> {
//   return transform_qmul(transform_qmul(q, vec4<f32>(v, 0.0)), transform_qconj(q)).xyz;
// }
fn transform_qrot(q: vec4<f32>, v: vec3<f32>) -> vec3<f32> {
  let t = 2.0 * cross(q.xyz, v);
  return v + q.w * t + cross(q.xyz, t);
}
)";

FeaturePtr Transform::create(bool applyView, bool applyProjection) {

  using namespace shader;
  using namespace shader::blocks;

  FeaturePtr feature = std::make_shared<Feature>();

  feature->requiredAttributes.requirePerVertexLocalBasis = true;

  auto expandInputVec = [](const char *_name, float lastComponent = 1.0f) {
    return std::make_unique<blocks::Custom>([=, name = std::string(_name)](shader::IGeneratorContext &ctx) {
      ctx.write("vec4<f32>(");
      ctx.readInput(name.c_str());

      auto &stageInputs = ctx.getDefinitions().inputs;
      auto it = stageInputs.find(name);
      if (it != stageInputs.end()) {
        auto &type = it->second;
        switch (type.numComponents) {
        case 2:
          ctx.write(fmt::format(".xy, 0.0, {:f})", lastComponent));
          break;
        case 3:
        case 4:
          ctx.write(fmt::format(".xyz, {:f})", lastComponent));
          break;
        default:
          ctx.pushError(formatError("Unsupported {} type", name));
          break;
        }
      }
    });
  };

  feature->shaderEntryPoints.emplace_back("transformLib", ProgrammableGraphicsStage::Vertex, Header(transformLibWgsl));

  feature->shaderEntryPoints.emplace_back("initLocalPosition", ProgrammableGraphicsStage::Vertex,
                                          WriteGlobal("localPosition", FieldTypes::Float4, expandInputVec("position")));

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

  BlockPtr localNormal = std::make_unique<WithInput>("normal", ReadInput("normal"), "vec3<f32>(0.0, 0.0, 1.0)");

  auto transformDirection = [](auto &&in) {
    return blocks::makeCompoundBlock("normalize((", ReadBuffer("invTransWorld", FieldTypes::Float4x4), "*", "vec4<f32>(",
                                     std::move(in), ".xyz, 0.0)", ").xyz)");
  };

  auto applyTBNTransform = blocks::makeCompoundBlock(
      WriteGlobal("worldNormal", FieldTypes::Float3, transformDirection(std::move(localNormal))),
      WithInput("tangent",
                makeCompoundBlock( //
                    WriteGlobal("worldTangent", FieldTypes::Float3, transformDirection(ReadInput("tangent"))),
                    WriteGlobal("biTangentSign", FieldTypes::Float, ReadInput("tangent"), ".w"))));

  auto &initWorldNormal =
      feature->shaderEntryPoints.emplace_back("initWorldNormal", ProgrammableGraphicsStage::Vertex, std::move(applyTBNTransform));
  initWorldNormal.dependencies.emplace_back("transformLib");

  auto &writePosition =
      feature->shaderEntryPoints.emplace_back("writePosition", ProgrammableGraphicsStage::Vertex,
                                              WriteOutput("position", FieldTypes::Float4, ReadGlobal("screenPosition")));
  writePosition.dependencies.emplace_back("initScreenPosition");

  auto &writeNormal = feature->shaderEntryPoints.emplace_back(
      "writeNormal", ProgrammableGraphicsStage::Vertex,
      makeCompoundBlock(WriteOutput("worldNormal", FieldTypes::Float3, ReadGlobal("worldNormal")),
                        WithInput("tangent", makeCompoundBlock( //
                                                 WriteOutput("worldTangent", FieldTypes::Float3, ReadGlobal("worldTangent")),
                                                 WriteOutput("biTangentSign", FieldTypes::Float, ReadGlobal("biTangentSign"))))));
  ;
  writeNormal.dependencies.emplace_back("initWorldNormal");

  auto &writeWorldPosition = feature->shaderEntryPoints.emplace_back(
      "writeWorldPosition", ProgrammableGraphicsStage::Vertex,
      WriteOutput("worldPosition", FieldTypes::Float3, ReadGlobal("worldPosition"), ".xyz"));
  writeWorldPosition.dependencies.emplace_back("initWorldPosition");

  return feature;
}
} // namespace features
} // namespace gfx