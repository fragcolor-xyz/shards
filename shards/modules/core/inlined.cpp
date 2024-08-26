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
ALWAYS_INLINE bool SHARDS_MODULE_FN(setInlineShardId)(Shard *shard, std::string_view name) {
  // Hook inline shards to override activation in runWire
  if (name == "Const") {
    shard->inlineShardId = InlineShard::CoreConst;
  } else if (name == "Pass") {
    shard->inlineShardId = InlineShard::NoopShard;
  } else if (name == "Comment") {
    shard->inlineShardId = InlineShard::NoopShard;
  } else if (name == "Input") {
    shard->inlineShardId = InlineShard::CoreInput;
  } else if (name == "Repeat") {
    shard->inlineShardId = InlineShard::CoreRepeat;
  } else if (name == "Swap") {
    shard->inlineShardId = InlineShard::CoreSwap;
  } else if (name == "Push") {
    shard->inlineShardId = InlineShard::CorePush;
  } else if (name == "Is") {
    shard->inlineShardId = InlineShard::CoreIs;
  } else if (name == "IsNot") {
    shard->inlineShardId = InlineShard::CoreIsNot;
  } else if (name == "IsMore") {
    shard->inlineShardId = InlineShard::CoreIsMore;
  } else if (name == "IsLess") {
    shard->inlineShardId = InlineShard::CoreIsLess;
  } else if (name == "IsMoreEqual") {
    shard->inlineShardId = InlineShard::CoreIsMoreEqual;
  } else if (name == "IsLessEqual") {
    shard->inlineShardId = InlineShard::CoreIsLessEqual;
  } else if (name == "IsTrue") {
    shard->inlineShardId = InlineShard::CoreIsTrue;
  } else if (name == "IsFalse") {
    shard->inlineShardId = InlineShard::CoreIsFalse;
  } else if (name == "IsNone") {
    shard->inlineShardId = InlineShard::CoreIsNone;
  } else if (name == "And") {
    shard->inlineShardId = InlineShard::CoreAnd;
  } else if (name == "Or") {
    shard->inlineShardId = InlineShard::CoreOr;
  } else if (name == "Not") {
    shard->inlineShardId = InlineShard::CoreNot;
  } else if (name == "Math.Add") {
    shard->inlineShardId = InlineShard::MathAdd;
  } else if (name == "Math.Subtract") {
    shard->inlineShardId = InlineShard::MathSubtract;
  } else if (name == "Math.Multiply") {
    shard->inlineShardId = InlineShard::MathMultiply;
  } else if (name == "Math.Divide") {
    shard->inlineShardId = InlineShard::MathDivide;
  } else if (name == "Math.Xor") {
    shard->inlineShardId = InlineShard::MathXor;
  } else if (name == "Math.And") {
    shard->inlineShardId = InlineShard::MathAnd;
  } else if (name == "Math.Or") {
    shard->inlineShardId = InlineShard::MathOr;
  } else if (name == "Math.Mod") {
    shard->inlineShardId = InlineShard::MathMod;
  } else if (name == "Math.LShift") {
    shard->inlineShardId = InlineShard::MathLShift;
  } else if (name == "Math.RShift") {
    shard->inlineShardId = InlineShard::MathRShift;
  }
  return shard->inlineShardId != 0;
}

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
    auto shard = reinterpret_cast<shards::AndRuntime *>(blk);
    return &shard->core.activate(context, input);
  }
  case InlineShard::CoreOr: {
    auto shard = reinterpret_cast<shards::OrRuntime *>(blk);
    return &shard->core.activate(context, input);
  }
  case InlineShard::CoreNot: {
    auto shard = reinterpret_cast<shards::NotRuntime *>(blk);
    return &shard->core.activate(context, input);
  }
  case InlineShard::CoreIs: {
    auto shard = reinterpret_cast<shards::IsRuntime *>(blk);
    return &shard->core.activate(context, input);
  }
  case InlineShard::CoreIsNot: {
    auto shard = reinterpret_cast<shards::IsNotRuntime *>(blk);
    return &shard->core.activate(context, input);
  }
  case InlineShard::CoreIsMore: {
    auto shard = reinterpret_cast<shards::IsMoreRuntime *>(blk);
    return &shard->core.activate(context, input);
  }
  case InlineShard::CoreIsLess: {
    auto shard = reinterpret_cast<shards::IsLessRuntime *>(blk);
    return &shard->core.activate(context, input);
  }
  case InlineShard::CoreIsMoreEqual: {
    auto shard = reinterpret_cast<shards::IsMoreEqualRuntime *>(blk);
    return &shard->core.activate(context, input);
  }
  case InlineShard::CoreIsLessEqual: {
    SHLOG_DEBUG("IsLessEqual");
    auto shard = reinterpret_cast<shards::IsLessEqualRuntime *>(blk);
    return &shard->core.activate(context, input);
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
    auto shard = reinterpret_cast<shards::RepeatRuntime *>(blk);
    return &shard->core.activate(context, input);
  }
  case InlineShard::CoreSwap: {
    auto shard = reinterpret_cast<shards::SwapRuntime *>(blk);
    return &shard->core.activate(context, input);
  }
  case InlineShard::MathAdd: {
    auto shard = reinterpret_cast<Math::AddRuntime *>(blk);
    return &shard->core.activate(context, input);
  }
  case InlineShard::MathSubtract: {
    auto shard = reinterpret_cast<Math::SubtractRuntime *>(blk);
    return &shard->core.activate(context, input);
  }
  case InlineShard::MathMultiply: {
    auto shard = reinterpret_cast<Math::MultiplyRuntime *>(blk);
    return &shard->core.activate(context, input);
  }
  case InlineShard::MathDivide: {
    auto shard = reinterpret_cast<Math::DivideRuntime *>(blk);
    return &shard->core.activate(context, input);
  }
  case InlineShard::MathXor: {
    auto shard = reinterpret_cast<Math::XorRuntime *>(blk);
    return &shard->core.activate(context, input);
  }
  case InlineShard::MathAnd: {
    auto shard = reinterpret_cast<Math::AndRuntime *>(blk);
    return &shard->core.activate(context, input);
  }
  case InlineShard::MathOr: {
    auto shard = reinterpret_cast<Math::OrRuntime *>(blk);
    return &shard->core.activate(context, input);
  }
  case InlineShard::MathMod: {
    auto shard = reinterpret_cast<Math::ModRuntime *>(blk);
    return &shard->core.activate(context, input);
  }
  case InlineShard::MathLShift: {
    auto shard = reinterpret_cast<Math::LShiftRuntime *>(blk);
    return &shard->core.activate(context, input);
  }
  case InlineShard::MathRShift: {
    auto shard = reinterpret_cast<Math::RShiftRuntime *>(blk);
    return &shard->core.activate(context, input);
  }
  case InlineShard::NotInline:
    return nullptr; // fail
    // default:
    // Don't add default case, we want to catch all cases
  }
  return nullptr;
}
} // namespace shards
