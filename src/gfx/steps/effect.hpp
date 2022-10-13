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
  static PipelineStepPtr create(RenderStepIO &&io, BlockPtr &&shader) {
    FeaturePtr feature = std::make_shared<Feature>();
    feature->shaderEntryPoints.emplace_back("effect_main", ProgrammableGraphicsStage::Fragment, std::move(shader));

    return makePipelineStep(RenderFullscreenStep{
        .features = withDefaultFullscreenFeatures(feature),
        .io = std::move(io),
    });
  }
};

struct Copy {
  static PipelineStepPtr create(const std::string &fieldName, WGPUTextureFormat dstFormat) {
    RenderStepIO io{
        .inputs = {fieldName},
        .outputs =
            {
                RenderStepIO::NamedOutput{
                    .name = fieldName,
                    .format = dstFormat,
                },
            },
    };
    auto step =
        Effect::create(std::move(io), makeCompoundBlock(WriteOutput(fieldName, FieldTypes::Float4, SampleTexture(fieldName))));
    return step;
  }
};
} // namespace gfx::steps

#endif /* D1F16AB2_B247_4571_B434_2A14302A56A3 */
