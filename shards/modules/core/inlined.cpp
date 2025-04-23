// This file contains the shard defintions that will be inlined in release builds
// When SHARDS_INLINED is disabled, this file is compiled separately
// Check the core CMakeLists

#include <cinttypes>
#include <shards/inlined.hpp>
#include <shards/core/inline.hpp>
#include <shards/core/module.hpp>
#include <shards/modules/core/core.hpp>
#include <shards/modules/core/math.hpp>
#include <shards/modules/core/memoize.hpp>

namespace shards {

ALWAYS_INLINE const SHVar *SHARDS_MODULE_FN(activateShardInline)(Shard *blk, SHContext *context, const SHVar &input) {
  auto enumValue = static_cast<shards::InlineShard::Type>(blk->inlineShardId);
  switch (enumValue) {
  case InlineShard::NoopShard:
    return &input;
  case InlineShard::CoreConst: {
    auto shard = reinterpret_cast<shards::ShardWrapper<Const> *>(blk);
    return &shard->shard._value;
  }
  case InlineShard::CoreAnd: {
    auto shard = reinterpret_cast<shards::ShardWrapper<shards::And> *>(blk);
    return &shard->shard.activate(context, input);
  }
  case InlineShard::CoreOr: {
    auto shard = reinterpret_cast<shards::ShardWrapper<shards::Or> *>(blk);
    return &shard->shard.activate(context, input);
  }
  case InlineShard::CoreNot: {
    auto shard = reinterpret_cast<shards::ShardWrapper<shards::Not> *>(blk);
    return &shard->shard.activate(context, input);
  }
  case InlineShard::CoreIs: {
    auto shard = reinterpret_cast<shards::ShardWrapper<shards::Is> *>(blk);
    return &shard->shard.activate(context, input);
  }
  case InlineShard::CoreIsNot: {
    auto shard = reinterpret_cast<shards::ShardWrapper<shards::IsNot> *>(blk);
    return &shard->shard.activate(context, input);
  }
  case InlineShard::CoreIsMore: {
    auto shard = reinterpret_cast<shards::ShardWrapper<shards::IsMore> *>(blk);
    return &shard->shard.activate(context, input);
  }
  case InlineShard::CoreIsLess: {
    auto shard = reinterpret_cast<shards::ShardWrapper<shards::IsLess> *>(blk);
    return &shard->shard.activate(context, input);
  }
  case InlineShard::CoreIsMoreEqual: {
    auto shard = reinterpret_cast<shards::ShardWrapper<shards::IsMoreEqual> *>(blk);
    return &shard->shard.activate(context, input);
  }
  case InlineShard::CoreIsLessEqual: {
    auto shard = reinterpret_cast<shards::ShardWrapper<shards::IsLessEqual> *>(blk);
    return &shard->shard.activate(context, input);
  }
  case InlineShard::CoreIsTrue: {
    auto shard = reinterpret_cast<shards::ShardWrapper<IsTrue> *>(blk);
    return &shard->shard.activate(context, input);
  }
  case InlineShard::CoreIsFalse: {
    auto shard = reinterpret_cast<shards::ShardWrapper<IsFalse> *>(blk);
    return &shard->shard.activate(context, input);
  }
  case InlineShard::CoreIsNone: {
    auto shard = reinterpret_cast<shards::ShardWrapper<IsNone> *>(blk);
    return &shard->shard.activate(context, input);
  }
  case InlineShard::CoreIsNotNone: {
    auto shard = reinterpret_cast<shards::ShardWrapper<IsNotNone> *>(blk);
    return &shard->shard.activate(context, input);
  }
  case InlineShard::CoreInput: {
    auto shard = reinterpret_cast<shards::ShardWrapper<Input> *>(blk);
    return &shard->shard.activate(context, input);
  }
  case InlineShard::CorePush: {
    auto shard = reinterpret_cast<shards::PushRuntime *>(blk);
    return &shard->core.activate(context, input);
  }
  case InlineShard::CoreGet: {
    auto shard = reinterpret_cast<shards::GetRuntime *>(blk);
    return shard->core._cell;
  }
  case InlineShard::CoreRefRegular: {
    auto shard = reinterpret_cast<shards::RefRuntime *>(blk);
    return &shard->core.activateRegular(context, input);
  }
  case InlineShard::CoreRefTable: {
    auto shard = reinterpret_cast<shards::RefRuntime *>(blk);
    return &shard->core.activateTable(context, input);
  }
  case InlineShard::CoreSetUpdateRegular: {
    auto shard = reinterpret_cast<shards::SetRuntime *>(blk);
    return &shard->core.activateRegular(context, input);
  }
  case InlineShard::CoreSetUpdateTable: {
    auto shard = reinterpret_cast<shards::SetRuntime *>(blk);
    return &shard->core.activateTable(context, input);
  }
  case InlineShard::CoreRepeat: {
    auto shard = reinterpret_cast<shards::ShardWrapper<shards::Repeat> *>(blk);
    return &shard->shard.activate(context, input);
  }
  case InlineShard::CoreSwap: {
    auto shard = reinterpret_cast<shards::ShardWrapper<shards::Swap> *>(blk);
    return &shard->shard.activate(context, input);
  }
  case InlineShard::MathAdd: {
    auto shard = reinterpret_cast<shards::ShardWrapper<shards::Math::Add> *>(blk);
    return &shard->shard.activate(context, input);
  }
  case InlineShard::MathSubtract: {
    auto shard = reinterpret_cast<shards::ShardWrapper<shards::Math::Subtract> *>(blk);
    return &shard->shard.activate(context, input);
  }
  case InlineShard::MathMultiply: {
    auto shard = reinterpret_cast<shards::ShardWrapper<shards::Math::Multiply> *>(blk);
    return &shard->shard.activate(context, input);
  }
  case InlineShard::MathDivide: {
    auto shard = reinterpret_cast<shards::ShardWrapper<shards::Math::Divide> *>(blk);
    return &shard->shard.activate(context, input);
  }
  case InlineShard::MathXor: {
    auto shard = reinterpret_cast<shards::ShardWrapper<shards::Math::Xor> *>(blk);
    return &shard->shard.activate(context, input);
  }
  case InlineShard::MathAnd: {
    auto shard = reinterpret_cast<shards::ShardWrapper<shards::Math::And> *>(blk);
    return &shard->shard.activate(context, input);
  }
  case InlineShard::MathOr: {
    auto shard = reinterpret_cast<shards::ShardWrapper<shards::Math::Or> *>(blk);
    return &shard->shard.activate(context, input);
  }
  case InlineShard::MathMod: {
    auto shard = reinterpret_cast<shards::ShardWrapper<shards::Math::Mod> *>(blk);
    return &shard->shard.activate(context, input);
  }
  case InlineShard::MathLShift: {
    auto shard = reinterpret_cast<shards::ShardWrapper<shards::Math::LShift> *>(blk);
    return &shard->shard.activate(context, input);
  }
  case InlineShard::MathRShift: {
    auto shard = reinterpret_cast<shards::ShardWrapper<shards::Math::RShift> *>(blk);
    return &shard->shard.activate(context, input);
  }
  case InlineShard::NotInline:
    return nullptr; // fail
    // default:
    // Don't add default case, we want to catch all cases
  }
  return nullptr;
}
} // namespace shards
