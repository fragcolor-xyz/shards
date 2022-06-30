#ifndef CC291073_D9DD_4B16_83A4_303777994C40
#define CC291073_D9DD_4B16_83A4_303777994C40

#include "../feature.hpp"
#include "../pipeline_step.hpp"
#include "../shader/blocks.hpp"

namespace gfx {
struct EguiRenderPass {
  static PipelineStepPtr createPipelineStep(DrawQueuePtr queue) {
    auto drawableStep = makeDrawablePipelineStep(RenderDrawablesStep{
        .drawQueue = queue,
        .features = std::vector<FeaturePtr>{createFeature()},
        .sortMode = SortMode::Queue,
    });
    return drawableStep;
  }

  static FeaturePtr createFeature() {
    using namespace shader::blocks;
    auto uiFeature = std::make_shared<Feature>();
    auto code = makeCompoundBlock();

    code->append(Header(R"(
fn linear_from_srgb(srgb: vec3<f32>) -> vec3<f32> {
  let srgb_bytes = srgb * 255.0;
  let cutoff = srgb_bytes < vec3<f32>(10.31475);
  let lower = srgb_bytes / vec3<f32>(3294.6);
  let higher = pow((srgb_bytes + vec3<f32>(14.025)) / vec3<f32>(269.025), vec3<f32>(2.4));
  return select(higher, lower, cutoff);
})"));
    code->appendLine("var vp = ", ReadBuffer("viewport", shader::FieldTypes::Float4, "view"));
    code->appendLine("var p1 = ", ReadInput("position"), " * (1.0 / vp.zw)");
    code->appendLine("p1.y = -p1.y;");
    code->appendLine("p1 = p1 * 2.0 + vec2<f32>(-1.0, 1.0)");
    code->appendLine("var p4 = vec4<f32>(p1.xy, 0.0, 1.0)");
    code->appendLine(WriteOutput("position", shader::FieldTypes::Float4, "p4"));
    code->appendLine("var color = ", ReadInput("color"));
    code->appendLine(WriteOutput("color", shader::FieldTypes::Float4, "vec4<f32>(linear_from_srgb(color.xyz), color.a)"));

    uiFeature->shaderEntryPoints.emplace_back("baseTransform", ProgrammableGraphicsStage::Vertex, std::move(code));

    shader::FieldType flagsFieldType = shader::FieldType(ShaderFieldBaseType::UInt32, 1);
    code = makeCompoundBlock();
    code->appendLine("var color = ", ReadInput("color"));
    code->appendLine("var texColor = ", SampleTexture("color"));
    code->appendLine("var isFont = ", ReadBuffer("flags", flagsFieldType), " & 1u");
    code->append("if(isFont != 0u) { texColor = vec4<f32>(texColor.xxx, texColor.x); }\n");
    code->appendLine(WriteOutput("color", shader::FieldTypes::Float4, "color * texColor"));
    uiFeature->shaderEntryPoints.emplace_back("color", ProgrammableGraphicsStage::Fragment, std::move(code));

    uiFeature->textureParams.emplace_back("color");
    uiFeature->shaderParams.emplace_back("flags", flagsFieldType, uint8_t(0));

    uiFeature->state.set_depthWrite(false);
    uiFeature->state.set_depthCompare(WGPUCompareFunction_Always);
    uiFeature->state.set_culling(false);
    uiFeature->state.set_blend(BlendState{
        .color = BlendComponent::AlphaPremultiplied,
        .alpha = BlendComponent::Opaque,
    });
    return uiFeature;
  }
};
} // namespace gfx

#endif /* CC291073_D9DD_4B16_83A4_303777994C40 */
