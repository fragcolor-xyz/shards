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
  static PipelineStepPtr create(RenderStepInput input, RenderStepOutput output) {
    auto blk = makeCompoundBlock();
    if (input.attachments.size() != output.attachments.size())
      throw std::runtime_error("Copy: number of input and output attachments must match");
    for (size_t i = 0; i < input.attachments.size(); i++) {
      auto &src = input.attachments[i];
      auto &dst = output.attachments[i];
      const auto &srcName = std::visit([&](auto &arg) -> const std::string & { return arg.name; }, src);
      const auto &dstName = std::visit([&](auto &arg) -> const std::string & { return arg.name; }, dst);
      blk->appendLine(WriteOutput(dstName, FieldTypes::Float4, SampleTexture(srcName)));
    }

    return Effect::create(std::move(input), std::move(output), std::move(blk));
  }
};
} // namespace gfx::steps

#endif /* D1F16AB2_B247_4571_B434_2A14302A56A3 */
