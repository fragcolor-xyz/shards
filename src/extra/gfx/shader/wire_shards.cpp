#include "translate_wrapper.hpp"

#include "shards/wires.hpp"
#include "composition.hpp"

using namespace shards;
namespace gfx::shader {

struct BaseRunnerTranslator {
  static void translate(BaseRunner *shard, TranslationContext &context) {
    auto input = context.takeWGSLTop();

    std::optional<FieldType> inputType = input ? std::make_optional(input->getType()) : std::nullopt;

    auto wire = shard->wire;
    auto &translatedWire = context.processWire(wire, inputType);

    BlockPtr callArgs;

    if (translatedWire.inputType) {
      assert(input);
      callArgs = input->toBlock();
    }

    auto callBlock = blocks::makeCompoundBlock();

    callBlock->append(fmt::format("{}(", translatedWire.functionName));
    if (callArgs)
      callBlock->append(std::move(callArgs));
    callBlock->append(")");

    if (translatedWire.outputType) {
      context.setWGSLTopVar(translatedWire.outputType.value(), std::move(callBlock));
    } else {
      context.addNew(std::move(callBlock));
    }
  }
};

void registerWireShards() {
  using RunWireDo = RunWire<false, RunWireMode::Inline>;

  // Literal copy-paste into shader code
  REGISTER_EXTERNAL_SHADER_SHARD(BaseRunnerTranslator, "Do", RunWireDo);
}
} // namespace gfx::shader