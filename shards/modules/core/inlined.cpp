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
  case InlineShard::CoreIs:
  case InlineShard::CoreIsNot: {
    auto shard = reinterpret_cast<shards::ShardWrapper<shards::Is> *>(blk);
    auto result = &shard->shard.activate(context, input);
    // save on ASM instructions by not inverting the result, big win for cache locality
    if (blk->inlineShardId == InlineShard::CoreIsNot) {
      shard->shard._output.payload.boolValue = !shard->shard._output.payload.boolValue;
    }
    return result;
  }
  case InlineShard::CoreIsMore:
  case InlineShard::CoreIsLessEqual: {
    auto shard = reinterpret_cast<shards::ShardWrapper<shards::IsMore> *>(blk);
    auto result = &shard->shard.activate(context, input);
    if (blk->inlineShardId == InlineShard::CoreIsLessEqual) {
      // !(a > b) == (a <= b)
      shard->shard._output.payload.boolValue = !shard->shard._output.payload.boolValue;
    }
    return result;
  }
  case InlineShard::CoreIsLess:
  case InlineShard::CoreIsMoreEqual: {
    auto shard = reinterpret_cast<shards::ShardWrapper<shards::IsLess> *>(blk);
    auto result = &shard->shard.activate(context, input);
    if (blk->inlineShardId == InlineShard::CoreIsMoreEqual) {
      // !(a < b) == (a >= b)
      shard->shard._output.payload.boolValue = !shard->shard._output.payload.boolValue;
    }
    return result;
  }
  case InlineShard::CoreIsTrue:
  case InlineShard::CoreIsFalse: {
    auto shard = reinterpret_cast<shards::ShardWrapper<IsTrue> *>(blk);
    auto result = &shard->shard.activate(context, input);
    if (blk->inlineShardId == InlineShard::CoreIsFalse) {
      // save on ASM instructions by not inverting the result, big win for cache locality
      shard->shard.output.payload.boolValue = !shard->shard.output.payload.boolValue;
    }
    return result;
  }
  case InlineShard::CoreIsNone:
  case InlineShard::CoreIsNotNone: {
    auto shard = reinterpret_cast<shards::ShardWrapper<IsNone> *>(blk);
    auto result = &shard->shard.activate(context, input);
    if (blk->inlineShardId == InlineShard::CoreIsNotNone) {
      // save on ASM instructions by not inverting the result, big win for cache locality
      shard->shard.output.payload.boolValue = !shard->shard.output.payload.boolValue;
    }
    return result;
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
  case InlineShard::MathAddInt64x2: {
    auto shard = reinterpret_cast<shards::ShardWrapper<shards::Math::Add> *>(blk);
    shard->shard.activateInt64x2(input);
    return &shard->shard._result;
  }
  case InlineShard::MathAddInt32x4: {
    auto shard = reinterpret_cast<shards::ShardWrapper<shards::Math::Add> *>(blk);
    shard->shard.activateInt32x4(input);
    return &shard->shard._result;
  }
  case InlineShard::MathAddFloat64x2: {
    auto shard = reinterpret_cast<shards::ShardWrapper<shards::Math::Add> *>(blk);
    shard->shard.activateFloat64x2(input);
    return &shard->shard._result;
  }
  case InlineShard::MathAddFloat32x4: {
    auto shard = reinterpret_cast<shards::ShardWrapper<shards::Math::Add> *>(blk);
    shard->shard.activateFloat32x4(input);
    return &shard->shard._result;
  }
  case InlineShard::MathSubtractInt64x2: {
    auto shard = reinterpret_cast<shards::ShardWrapper<shards::Math::Subtract> *>(blk);
    shard->shard.activateInt64x2(input);
    return &shard->shard._result;
  }
  case InlineShard::MathSubtractInt32x4: {
    auto shard = reinterpret_cast<shards::ShardWrapper<shards::Math::Subtract> *>(blk);
    shard->shard.activateInt32x4(input);
    return &shard->shard._result;
  }
  case InlineShard::MathSubtractFloat64x2: {
    auto shard = reinterpret_cast<shards::ShardWrapper<shards::Math::Subtract> *>(blk);
    shard->shard.activateFloat64x2(input);
    return &shard->shard._result;
  }
  case InlineShard::MathSubtractFloat32x4: {
    auto shard = reinterpret_cast<shards::ShardWrapper<shards::Math::Subtract> *>(blk);
    shard->shard.activateFloat32x4(input);
    return &shard->shard._result;
  }
  case InlineShard::MathMultiplyInt64x2: {
    auto shard = reinterpret_cast<shards::ShardWrapper<shards::Math::Multiply> *>(blk);
    shard->shard.activateInt64x2(input);
    return &shard->shard._result;
  }
  case InlineShard::MathMultiplyInt32x4: {
    auto shard = reinterpret_cast<shards::ShardWrapper<shards::Math::Multiply> *>(blk);
    shard->shard.activateInt32x4(input);
    return &shard->shard._result;
  }
  case InlineShard::MathMultiplyFloat64x2: {
    auto shard = reinterpret_cast<shards::ShardWrapper<shards::Math::Multiply> *>(blk);
    shard->shard.activateFloat64x2(input);
    return &shard->shard._result;
  }
  case InlineShard::MathMultiplyFloat32x4: {
    auto shard = reinterpret_cast<shards::ShardWrapper<shards::Math::Multiply> *>(blk);
    shard->shard.activateFloat32x4(input);
    return &shard->shard._result;
  }
  case InlineShard::MathDivideInt64x2: {
    auto shard = reinterpret_cast<shards::ShardWrapper<shards::Math::Divide> *>(blk);
    shard->shard.activateInt64x2(input);
    return &shard->shard._result;
  }
  case InlineShard::MathDivideInt32x4: {
    auto shard = reinterpret_cast<shards::ShardWrapper<shards::Math::Divide> *>(blk);
    shard->shard.activateInt32x4(input);
    return &shard->shard._result;
  }
  case InlineShard::MathDivideFloat64x2: {
    auto shard = reinterpret_cast<shards::ShardWrapper<shards::Math::Divide> *>(blk);
    shard->shard.activateFloat64x2(input);
    return &shard->shard._result;
  }
  case InlineShard::MathDivideFloat32x4: {
    auto shard = reinterpret_cast<shards::ShardWrapper<shards::Math::Divide> *>(blk);
    shard->shard.activateFloat32x4(input);
    return &shard->shard._result;
  }
  case InlineShard::MathXorInt64x2: {
    auto shard = reinterpret_cast<shards::ShardWrapper<shards::Math::Xor> *>(blk);
    shard->shard.activateInt64x2(input);
    return &shard->shard._result;
  }
  case InlineShard::MathXorInt32x4: {
    auto shard = reinterpret_cast<shards::ShardWrapper<shards::Math::Xor> *>(blk);
    shard->shard.activateInt32x4(input);
    return &shard->shard._result;
  }
  case InlineShard::MathAndInt64x2: {
    auto shard = reinterpret_cast<shards::ShardWrapper<shards::Math::And> *>(blk);
    shard->shard.activateInt64x2(input);
    return &shard->shard._result;
  }
  case InlineShard::MathAndInt32x4: {
    auto shard = reinterpret_cast<shards::ShardWrapper<shards::Math::And> *>(blk);
    shard->shard.activateInt32x4(input);
    return &shard->shard._result;
  }
  case InlineShard::MathOrInt64x2: {
    auto shard = reinterpret_cast<shards::ShardWrapper<shards::Math::Or> *>(blk);
    shard->shard.activateInt64x2(input);
    return &shard->shard._result;
  }
  case InlineShard::MathOrInt32x4: {
    auto shard = reinterpret_cast<shards::ShardWrapper<shards::Math::Or> *>(blk);
    shard->shard.activateInt32x4(input);
    return &shard->shard._result;
  }
  case InlineShard::MathModInt64x2: {
    auto shard = reinterpret_cast<shards::ShardWrapper<shards::Math::Mod> *>(blk);
    shard->shard.activateInt64x2(input);
    return &shard->shard._result;
  }
  case InlineShard::MathModInt32x4: {
    auto shard = reinterpret_cast<shards::ShardWrapper<shards::Math::Mod> *>(blk);
    shard->shard.activateInt32x4(input);
    return &shard->shard._result;
  }
  case InlineShard::NotInline:
    return blk->activate(blk, context, &input);
    // default:
    // Don't add default case, we want to catch all cases
  }
  return nullptr;
}
} // namespace shards
