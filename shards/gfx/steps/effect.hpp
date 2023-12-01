#ifndef D1F16AB2_B247_4571_B434_2A14302A56A3
#define D1F16AB2_B247_4571_B434_2A14302A56A3

#include "../pipeline_step.hpp"
#include "../feature.hpp"
#include "../features/transform.hpp"
#include "../shader/blocks.hpp"
#include "../shader/types.hpp"
#include "defaults.hpp"

namespace gfx::steps {
using namespace shader::blocks;
using shader::FieldTypes;

struct Effect {
  static PipelineStepPtr create(RenderStepInput &&input, RenderStepOutput &&output, BlockPtr &&shader) {
    FeaturePtr feature = std::make_shared<Feature>();
    feature->shaderEntryPoints.emplace_back("effect_main", ProgrammableGraphicsStage::Fragment, std::move(shader));

    return makePipelineStep(RenderFullscreenStep{
        .features = withDefaultFullscreenFeatures(feature),
        .input = std::move(input),
        .output = std::move(output),
    });
  }
};

struct Copy {
  static PipelineStepPtr create(const std::string &fieldName, WGPUTextureFormat dstFormat) {
    RenderStepInput in;
    in.attachments.push_back(RenderStepInput::Named{
        .name = fieldName,
    });
    auto step = Effect::create(std::move(in),
                               makeRenderStepOutput(RenderStepOutput::Named{
                                   .name = fieldName,
                                   .format = dstFormat,
                               }),
                               makeCompoundBlock(WriteOutput(fieldName, FieldTypes::Float4, SampleTexture(fieldName))));
    return step;
  }
};
} // namespace gfx::steps

#endif /* D1F16AB2_B247_4571_B434_2A14302A56A3 */
