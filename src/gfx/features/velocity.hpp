#ifndef C6156B57_AD63_45C1_86D1_ABA4F1815677
#define C6156B57_AD63_45C1_86D1_ABA4F1815677

#include <gfx/enums.hpp>
#include <gfx/feature.hpp>
#include <gfx/params.hpp>
#include <gfx/shader/blocks.hpp>
#include <gfx/pipeline_builder.hpp>
#include <memory>

namespace gfx {
namespace features {

// Outputs per-object screen-space velocity into the 'velocity' output and global
struct Velocity {
  // Scaling factor for velocity values stored in the framebuffer
  // output = (screen pixels/s) * scaling factor
  static constexpr double scalingFactor = 32.0;

  static inline FeaturePtr create(bool applyView = true, bool applyProjection = true) {
    using namespace shader;
    using namespace shader::blocks;

    struct PipelineModifier : public IPipelineModifier {
      void buildPipeline(PipelineBuilder &builder) {
        auto &objectBinding = builder.getOrCreateBufferBinding("object");
        objectBinding.frequency = BindingFrequency::Draw;
        objectBinding.layoutBuilder.push("previousWorld", FieldTypes::Float4x4);

        auto &viewBinding = builder.getOrCreateBufferBinding("view");
        viewBinding.frequency = BindingFrequency::View;
        viewBinding.layoutBuilder.push("previousView", FieldTypes::Float4x4);
      }
    };

    FeaturePtr feature = std::make_shared<Feature>();
    feature->pipelineModifier = std::make_shared<PipelineModifier>();
    feature->drawableParameterGenerators.emplace_back([](const FeatureCallbackContext &ctx, IParameterCollector &collector) {
      collector.setParam("previousWorld", ctx.cachedDrawable->previousTransform);
    });

    feature->viewParameterGenerators.emplace_back([](const FeatureCallbackContext &ctx, IParameterCollector &collector) {
      collector.setParam("previousView", ctx.cachedView->previousViewTransform);
    });

    {
      auto code = blocks::makeCompoundBlock();
      code->appendLine("let localPosition = ", ReadGlobal("localPosition"));

      code->append("let prevScreenPosition = ");
      if (applyProjection)
        code->append(ReadBuffer("proj", FieldTypes::Float4x4, "view"), "*");
      if (applyView)
        code->append(ReadBuffer("previousView", FieldTypes::Float4x4, "view"), "*");
      code->append(ReadBuffer("previousWorld", FieldTypes::Float4x4, "object"), "*");
      code->appendLine("localPosition");
      code->appendLine(WriteOutput("previousPosition", FieldTypes::Float4, "prevScreenPosition"));

      auto &writePreviousPosition =
          feature->shaderEntryPoints.emplace_back("writePreviousPosition", ProgrammableGraphicsStage::Vertex, std::move(code));
      writePreviousPosition.dependencies.emplace_back("initWorldPosition");
    }

    {
      auto code = blocks::makeCompoundBlock();
      // NOTE: Fragment shader position input is in framebuffer space
      // https://www.w3.org/TR/WGSL/#builtin-values
      code->appendLine("let screenPosition = ", ReadInput("position"));
      code->appendLine("let viewport = ", ReadBuffer("viewport", FieldTypes::Float4, "view"));
      code->appendLine("let ndc = (screenPosition.xy - viewport.xy) / viewport.zw * vec2<f32>(2.0, -2.0) - vec2<f32>(1.0, -1.0)");

      code->appendLine("let prevNdc = ", ReadInput("previousPosition"));
      code->appendLine("let ndcVelocity = (ndc.xy - prevNdc.xy/prevNdc.w)");
      code->appendLine(WriteGlobal("velocity", FieldTypes::Float2, "ndcVelocity"));

      // Apply scale to fit in output precision
      code->appendLine(WithOutput("velocity", WriteOutput("velocity", FieldTypes::Float2, ReadGlobal("velocity"),
                                                          fmt::format(" * {:0.2}", scalingFactor))));

      feature->shaderEntryPoints.emplace_back("initVelocity", ProgrammableGraphicsStage::Fragment, std::move(code));
    }

    return feature;
  }
};

} // namespace features
} // namespace gfx

#endif /* C6156B57_AD63_45C1_86D1_ABA4F1815677 */
