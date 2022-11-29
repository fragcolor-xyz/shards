#ifndef CD187556_E38B_4180_B6B9_B72301E8299E
#define CD187556_E38B_4180_B6B9_B72301E8299E

#include "composition.hpp"
#include "translator.hpp"
#include "translator_utils.hpp"
#include "../shards_utils.hpp"
#include "spdlog/spdlog.h"
#include <common_types.hpp>
#include <foundation.hpp>
#include <runtime.hpp>
#include <gfx/shader/log.hpp>
#include <gfx/shader/block.hpp>

using namespace shards;
namespace gfx::shader {
static auto logger = shader::getLogger();

static thread_local ShaderCompositionContext *compositionContext{};

ShaderCompositionContext &ShaderCompositionContext::get() {
  if (!compositionContext)
    throw ComposeError("Shader shards can not be used outside of a shader");
  return *compositionContext;
}

void ShaderCompositionContext::setContext(ShaderCompositionContext *context) { compositionContext = context; }

struct DynamicBlockFromShards : public blocks::Block {
  std::vector<ShardPtr> shards;

  DynamicBlockFromShards(std::vector<ShardPtr> &&shards) : shards(std::move(shards)) {}

  virtual void apply(IGeneratorContext &context) const {
    auto &definitions = context.getDefinitions();

    // Compose the shards
    auto composeCallback = [](const struct Shard *errorShard, SHString errorTxt, SHBool nonfatalWarning, void *userData) {
      auto shardName = errorShard->name(const_cast<Shard *>(errorShard));
      throw formatException("Failed to compose shader shards: {} ({})", errorTxt, shardName);
    };

    SHInstanceData instanceData{};
    std::vector<SHExposedTypeInfo> exposedTypes;
    for (auto &v : definitions.globals) {
      auto &entry = exposedTypes.emplace_back();
      entry.name = v.first.c_str();
      entry.global = true;
      entry.exposedType = Type{fieldTypeToShardsType(v.second)};
      entry.isMutable = true;
    }
    instanceData.shared = SHExposedTypesInfo{
        .elements = exposedTypes.data(),
        .len = uint32_t(exposedTypes.size()),
    };

    ShaderCompositionContext shaderCompositionContext(context);
    SHComposeResult result = ShaderCompositionContext::withContext(
        shaderCompositionContext, [&]() { return composeWire(shards, composeCallback, nullptr, instanceData); });

    if (result.failed)
      throw formatException("Failed to compose shader shards");

    // Process shards by translator
    shader::TranslationContext shaderCtx;
    for (ShardPtr shard : shards) {
      shaderCtx.processShard(shard);
    }

    auto compiledBlock = std::move(shaderCtx.root);
    compiledBlock->apply(context);
  }

  virtual std::unique_ptr<Block> clone() { return std::make_unique<DynamicBlockFromShards>(std::vector<ShardPtr>(shards)); }
};

void applyShaderEntryPoint(SHContext *context, shader::EntryPoint &entryPoint, const SHVar &input) {
  checkType(input.valueType, SHType::Seq, ":Shaders EntryPoint");

  // Check input type is a shard sequence
  std::vector<ShardPtr> shards;
  for (SHVar &shardVar : IterableSeq(input)) {
    checkType(shardVar.valueType, SHType::ShardRef, ":Shaders EntryPoint");
    shards.push_back(shardVar.payload.shardValue);
  }

  entryPoint.code = std::make_unique<DynamicBlockFromShards>(std::move(shards));
}

} // namespace gfx::shader

#endif /* CD187556_E38B_4180_B6B9_B72301E8299E */
