#include <shards/flow.hpp>
#include "translate_wrapper.hpp"
#include <gfx/shader/fmt.hpp>

using namespace shards;
namespace gfx::shader {
struct SubTranslator {
  static void translate(Sub *shard, TranslationContext &context) {
    auto restoreTop = WGSLBlock(context.wgslTop->getType(), context.wgslTop->toBlock());

    auto &shardsSeq = shard->_shards.shards();
    for (size_t i = 0; i < shardsSeq.len; i++) {
      ShardPtr shard = shardsSeq.elements[i];
      context.processShard(shard);
    }

    context.setWGSLTop<WGSLBlock>(std::move(restoreTop));
  }
};

void registerFlowShards() { REGISTER_EXTERNAL_SHADER_SHARD(SubTranslator, "Sub", shards::Sub); }
} // namespace gfx::shader
