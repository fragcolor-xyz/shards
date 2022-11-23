// This file contains the shard defintions that will be inlined in release builds
// In debug this file is compiled separately

#include "inlined.hpp"
#include <cinttypes>

#include "shards/core.hpp"
#include "shards/math.hpp"

namespace shards {

SHARDS_CONDITIONAL_INLINE void setInlineShardId(Shard *shard, std::string_view name) {
  // Hook inline shards to override activation in runWire
  if (name == "Const") {
    shard->inlineShardId = InlineShard::CoreConst;
  } else if (name == "Pass") {
    shard->inlineShardId = InlineShard::NoopShard;
  } else if (name == "OnCleanup") {
    shard->inlineShardId = InlineShard::NoopShard;
  } else if (name == "Comment") {
    shard->inlineShardId = InlineShard::NoopShard;
  } else if (name == "Input") {
    shard->inlineShardId = InlineShard::CoreInput;
  } else if (name == "Pause") {
    shard->inlineShardId = InlineShard::CoreSleep;
  } else if (name == "ForRange") {
    shard->inlineShardId = InlineShard::CoreForRange;
  } else if (name == "Repeat") {
    shard->inlineShardId = InlineShard::CoreRepeat;
  } else if (name == "Once") {
    shard->inlineShardId = InlineShard::CoreOnce;
  } else if (name == "Set") {
    shard->inlineShardId = InlineShard::CoreSet;
  } else if (name == "Update") {
    shard->inlineShardId = InlineShard::CoreUpdate;
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
  // Unary math is dealt inside math.hpp compose
}

SHARDS_CONDITIONAL_INLINE bool activateShardInline(Shard *blk, SHContext *context, const SHVar &input, SHVar &output) {
  switch (blk->inlineShardId) {
  case InlineShard::NoopShard:
    output = input;
    break;
  case InlineShard::CoreConst: {
    auto shard = reinterpret_cast<shards::ShardWrapper<Const> *>(blk);
    output = shard->shard._value;
    break;
  }
  case InlineShard::CoreAnd: {
    auto shard = reinterpret_cast<shards::AndRuntime *>(blk);
    output = shard->core.activate(context, input);
    break;
  }
  case InlineShard::CoreOr: {
    auto shard = reinterpret_cast<shards::OrRuntime *>(blk);
    output = shard->core.activate(context, input);
    break;
  }
  case InlineShard::CoreNot: {
    auto shard = reinterpret_cast<shards::NotRuntime *>(blk);
    output = shard->core.activate(context, input);
    break;
  }
  case InlineShard::CoreInput: {
    auto shard = reinterpret_cast<shards::ShardWrapper<Input> *>(blk);
    output = shard->shard.activate(context, input);
    break;
  }
  case InlineShard::CorePush: {
    auto shard = reinterpret_cast<shards::PushRuntime *>(blk);
    output = shard->core.activate(context, input);
    break;
  }
  case InlineShard::CoreGet: {
    auto shard = reinterpret_cast<shards::GetRuntime *>(blk);
    output = *shard->core._cell;
    break;
  }
  case InlineShard::CoreSet: {
    auto shard = reinterpret_cast<shards::SetRuntime *>(blk);
    output = shard->core.activate(context, input);
    break;
  }
  case InlineShard::CoreRefTable: {
    auto shard = reinterpret_cast<shards::RefRuntime *>(blk);
    output = shard->core.activateTable(context, input);
    break;
  }
  case InlineShard::CoreRefRegular: {
    auto shard = reinterpret_cast<shards::RefRuntime *>(blk);
    output = shard->core.activateRegular(context, input);
    break;
  }
  case InlineShard::CoreUpdate: {
    auto shard = reinterpret_cast<shards::UpdateRuntime *>(blk);
    output = shard->core.activate(context, input);
    break;
  }
  case InlineShard::CoreSwap: {
    auto shard = reinterpret_cast<shards::SwapRuntime *>(blk);
    output = shard->core.activate(context, input);
    break;
  }
  default:
    return false;
  }
  return true;
}

} // namespace shards
