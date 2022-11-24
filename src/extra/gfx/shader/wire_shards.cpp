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

    for (auto &req : shard->requiredVariables()) {
      SPDLOG_LOGGER_INFO(context.logger, "- Required variable: {}", req.name);
      auto arg = context.reference(req.name);
    }

    BlockPtr inputCallArg;

    if (translatedWire.inputType) {
      assert(input);
      inputCallArg = input->toBlock();
    }

    // Build function call argument list
    auto callBlock = blocks::makeCompoundBlock();
    callBlock->append(fmt::format("{}(", translatedWire.functionName));

    // Pass input as first argument
    if (inputCallArg)
      callBlock->append(std::move(inputCallArg));

    // Pass captured variables
    if (!translatedWire.arguments.empty()) {
      bool addSeparator = (bool)inputCallArg;
      for (auto &arg : translatedWire.arguments) {
        if (addSeparator)
          callBlock->append(", ");
        callBlock->append(context.reference(arg.shardsName).toBlock());
        addSeparator = true;
      }
    }

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