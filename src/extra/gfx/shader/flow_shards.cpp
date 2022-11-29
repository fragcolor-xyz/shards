#include <shards/flow.hpp>
#include "translate_wrapper.hpp"
#include "translator_utils.hpp"
#include <gfx/shader/fmt.hpp>

using namespace shards;
namespace gfx::shader {
struct SubTranslator {
  static void translate(Sub *shard, TranslationContext &context) {
    auto restoreTop = WGSLBlock(context.wgslTop->getType(), context.wgslTop->toBlock());

    processShardsVar(shard->_shards, context);

    context.setWGSLTop<WGSLBlock>(std::move(restoreTop));
  }
};

struct IfTranslator {
  static void translate(IfBlock *shard, TranslationContext &context) {
    auto &input = context.wgslTop;
    FieldType inputType = input->getType();
    BlockPtr inputBlock = input->toBlock();

    // Evaluate condition
    processShardsVar(shard->_cond, context);
    auto cmp = context.takeWGSLTop();

    context.addNew(blocks::makeBlock<blocks::Direct>("if("));
    context.addNew(cmp->toBlock());
    context.addNew(blocks::makeBlock<blocks::Direct>(") {\n"));

    // Then block
    context.enterNew(blocks::makeCompoundBlock());
    context.setWGSLTop<WGSLBlock>(inputType, inputBlock->clone());
    processShardsVar(shard->_then, context);
    context.leave();

    if (shard->_else.shards().len > 0) {
      context.addNew(blocks::makeBlock<blocks::Direct>("} else {\n"));

      // Else block
      context.enterNew(blocks::makeCompoundBlock());
      context.setWGSLTop<WGSLBlock>(inputType, inputBlock->clone());
      processShardsVar(shard->_else, context);
      context.leave();
    }
    context.addNew(blocks::makeBlock<blocks::Direct>("}\n"));

    if (shard->_passth) {
      context.setWGSLTop<WGSLBlock>(inputType, std::move(inputBlock));
    } else {
      context.setWGSLTop<WGSLBlock>(cmp->getType(), cmp->toBlock());
    }
  }
};

struct ForRangeTranslator {
  static void translate(ForRangeShard *shard, TranslationContext &context) {
    auto from = translateParamVar(shard->_from, context);
    auto to = translateParamVar(shard->_to, context);

    std::string indexVarName = context.getUniqueVariableName("index");

    context.enterNew(blocks::makeCompoundBlock());
    context.addNew(blocks::makeBlock<blocks::Direct>(fmt::format("for(var {} = ", indexVarName)));
    context.addNew(from->toBlock());
    context.addNew(blocks::makeBlock<blocks::Direct>(fmt::format("; {} < ", indexVarName)));
    context.addNew(to->toBlock());
    context.addNew(blocks::makeBlock<blocks::Direct>(fmt::format("; {}++) {{", indexVarName)));

    // Pass index as input
    context.setWGSLTop<WGSLBlock>(FieldTypes::Int32, blocks::makeBlock<blocks::Direct>(indexVarName));

    // Loop body
    processShardsVar(shard->_shards, context);

    context.addNew(blocks::makeBlock<blocks::Direct>("}"));
    context.leave();
  }
};

void registerFlowShards() {
  REGISTER_EXTERNAL_SHADER_SHARD(SubTranslator, "Sub", shards::Sub);
  REGISTER_EXTERNAL_SHADER_SHARD(IfTranslator, "If", shards::IfBlock);
  REGISTER_EXTERNAL_SHADER_SHARD(ForRangeTranslator, "ForRange", shards::ForRangeShard);
}
} // namespace gfx::shader
