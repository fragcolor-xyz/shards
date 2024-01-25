#include "translate_wrapper.hpp"
#include "translator_utils.hpp"

#include <shards/modules/core/wires.hpp>
#include "composition.hpp"

using namespace shards;
namespace gfx::shader {

struct BaseRunnerTranslator {
  static void translate(BaseRunner *shard, TranslationContext &context) {
    auto input = context.takeWGSLTop();
    std::optional<Type> inputType = input ? std::make_optional(input->getType()) : std::nullopt;

    auto wire = shard->wire;
    const TranslatedFunction &translatedFunction = context.processWire(wire, inputType);

    auto output = generateFunctionCall(translatedFunction, input, context);
    if (translatedFunction.outputType) {
      context.setWGSLTopVar(output->getType(), output->toBlock());
    } else {
      context.addNew(output->toBlock());
    }
  }
};

void registerWireShards() {
  using RunWireDo = RunWire<false, RunWireMode::Inline>;

  // Literal copy-paste into shader code
  REGISTER_EXTERNAL_SHADER_SHARD(BaseRunnerTranslator, "Do", RunWireDo);
}
} // namespace gfx::shader