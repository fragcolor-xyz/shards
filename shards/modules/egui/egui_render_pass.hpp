#ifndef CC291073_D9DD_4B16_83A4_303777994C40
#define CC291073_D9DD_4B16_83A4_303777994C40

#include <gfx/feature.hpp>
#include <gfx/pipeline_step.hpp>
#include <gfx/shader/blocks.hpp>
#include <gfx/features/transform.hpp>

namespace gfx {
struct EguiTransformFeature {
  static FeaturePtr create() {
    using namespace shader::blocks;
    auto feature = std::make_shared<Feature>();
    auto code = makeCompoundBlock();

    code->appendLine("var vp = ", ReadBuffer("viewport", shader::Types::Float4, "view"));
    code->appendLine("var p1 = ", ReadInput("position"), " * (1.0 / vp.zw)");
    code->appendLine("p1.y = -p1.y;");
    code->appendLine("p1 = p1 * 2.0 + vec2<f32>(-1.0, 1.0)");
    code->appendLine("var p4 = vec4<f32>(p1.xy, 0.0, 1.0)");
    code->appendLine(WriteOutput("position", shader::Types::Float4, "p4"));

    auto &transform =
        feature->shaderEntryPoints.emplace_back("writeUiTransform", ProgrammableGraphicsStage::Vertex, std::move(code));
    // NOTE: Override Transform's writePosition
    transform.dependencies.emplace_back("writePosition");

    return feature;
  }
};

struct EguiColorFeature {
  static FeaturePtr create() {
    using namespace shader::blocks;
    auto feature = std::make_shared<Feature>();
    auto code = makeCompoundBlock();

    code->append(Header(R"(
fn egui_linear_from_srgb(srgb: vec3<f32>) -> vec3<f32> {
  let srgb_bytes = srgb * 255.0;
  let cutoff = srgb_bytes < vec3<f32>(10.31475);
  let lower = srgb_bytes / vec3<f32>(3294.6);
  let higher = pow((srgb_bytes + vec3<f32>(14.025)) / vec3<f32>(269.025), vec3<f32>(2.4));
  return select(higher, lower, cutoff);
})"));

    code->appendLine("var color = ", ReadInput("color"));
    code->appendLine(WriteOutput("color", shader::Types::Float4, "vec4<f32>(egui_linear_from_srgb(color.xyz), color.a)"));

    auto &writeVertColor =
        feature->shaderEntryPoints.emplace_back("writeUiColor", ProgrammableGraphicsStage::Vertex, std::move(code));
    // NOTE: Override BaseColor's writeColor
    writeVertColor.dependencies.emplace_back("writeColor");

    shader::NumType flagsFieldType = shader::NumType(ShaderFieldBaseType::UInt32, 1);
    code = makeCompoundBlock();
    code->appendLine("var color = ", ReadInput("color"));
    code->appendLine("var texColor = ", SampleTexture("color"));
    code->appendLine("var isFont = ", ReadBuffer("flags", flagsFieldType), " & 1u");
    code->append("if(isFont != 0u) { texColor = vec4<f32>(texColor.xxx, texColor.x); }\n");
    code->appendLine(WriteOutput("color", shader::Types::Float4, "color * texColor"));

    auto &writeFragColor =
        feature->shaderEntryPoints.emplace_back("writeUiColor", ProgrammableGraphicsStage::Fragment, std::move(code));
    // NOTE: Override BaseColor's writeColor
    writeFragColor.dependencies.emplace_back("writeColor");

    feature->textureParams.emplace("color", TextureParamDecl());
    feature->shaderParams.emplace("flags", NumParamDecl(flagsFieldType, uint32_t(0)));

    feature->state.set_blend(BlendState{
        .color = BlendComponent::AlphaPremultiplied,
        .alpha = BlendComponent::Opaque,
    });
    feature->state.set_depthWrite(false);
    feature->state.set_depthCompare(WGPUCompareFunction_Less);
    feature->state.set_culling(false);
    return feature;
  }
};

struct EguiRenderPass {
  static PipelineStepPtr createPipelineStep(DrawQueuePtr queue) {
    auto drawableStep = makePipelineStep(createDrawableStep(queue));
    return drawableStep;
  }

  static RenderDrawablesStep createDrawableStep(DrawQueuePtr queue) {
    return RenderDrawablesStep{
        .drawQueue = queue,
        .sortMode = SortMode::Queue,
        .features =
            std::vector<FeaturePtr>{
                EguiTransformFeature::create(),
            },
    };
  }
};
} // namespace gfx

#endif /* CC291073_D9DD_4B16_83A4_303777994C40 */
