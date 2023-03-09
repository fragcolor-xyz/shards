// This file contains the shard defintions that will be inlined in release builds
// When SHARDS_INLINED is disabled, this file is compiled separately
// Check the core CMakeLists

#include "inlined.hpp"
#include <cinttypes>

#include "shards/core.hpp"
#include "shards/math.hpp"

namespace shards {
void setInlineShardId(Shard *shard, std::string_view name) {
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

FLATTEN SHARDS_CONDITIONAL_INLINE bool activateShardInline(Shard *blk, SHContext *context, const SHVar &input, SHVar &output) {
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
  case InlineShard::CoreRefRegular: {
    auto shard = reinterpret_cast<shards::RefRuntime *>(blk);
    output = shard->core.activateRegular(context, input);
    break;
  }
  case InlineShard::CoreRefTable: {
    auto shard = reinterpret_cast<shards::RefRuntime *>(blk);
    output = shard->core.activateTable(context, input);
    break;
  }
  case InlineShard::CoreSetUpdateRegular: {
    auto shard = reinterpret_cast<shards::SetRuntime *>(blk);
    output = shard->core.activate(context, input);
    break;
  }
  case InlineShard::CoreSetUpdateTable: {
    auto shard = reinterpret_cast<shards::SetRuntime *>(blk);
    output = shard->core.activateTable(context, input);
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

void hash_update(const SHVar &var, void *state);

FLATTEN SHARDS_CONDITIONAL_INLINE inline SHVar activateShard(Shard *blk, SHContext *context, const SHVar &input) {
  ZoneScoped;
  ZoneName(blk->name(blk), blk->nameLength);

  SHVar output;
  if (!activateShardInline(blk, context, input, output))
    output = blk->activate(blk, context, &input);
  return output;
}

template <typename T, bool HANDLES_RETURN, bool HASHED>
FLATTEN SHARDS_CONDITIONAL_INLINE SHWireState shardsActivation(T &shards, SHContext *context, const SHVar &wireInput, SHVar &output,
                                                   SHVar *outHash = nullptr) noexcept {
  XXH3_state_s hashState; // optimized out in release if not HASHED
  if constexpr (HASHED) {
    assert(outHash);
    XXH3_INITSTATE(&hashState);
    XXH3_128bits_reset_withSecret(&hashState, CUSTOM_XXH3_kSecret, XXH_SECRET_DEFAULT_SIZE);
  }
  DEFER(if constexpr (HASHED) {
    auto digest = XXH3_128bits_digest(&hashState);
    outHash->valueType = SHType::Int2;
    outHash->payload.int2Value[0] = int64_t(digest.low64);
    outHash->payload.int2Value[1] = int64_t(digest.high64);
    SHLOG_TRACE("Hash digested {}", *outHash);
  });

  // store initial input
  auto input = wireInput;

  // find len based on shards type
  size_t len;
  if constexpr (std::is_same<T, Shards>::value || std::is_same<T, SHSeq>::value) {
    len = shards.len;
  } else if constexpr (std::is_same<T, std::vector<ShardPtr>>::value) {
    len = shards.size();
  } else {
    len = 0;
    SHLOG_FATAL("Unreachable shardsActivation case");
  }

  for (size_t i = 0; i < len; i++) {
    ShardPtr blk;
    if constexpr (std::is_same<T, Shards>::value) {
      blk = shards.elements[i];
    } else if constexpr (std::is_same<T, SHSeq>::value) {
      blk = shards.elements[i].payload.shardValue;
    } else if constexpr (std::is_same<T, std::vector<ShardPtr>>::value) {
      blk = shards[i];
    } else {
      blk = nullptr;
      SHLOG_FATAL("Unreachable shardsActivation case");
    }

    if constexpr (HASHED) {
      const auto shardHash = blk->hash(blk);
      SHLOG_TRACE("Hashing shard {}", shardHash);
      XXH3_128bits_update(&hashState, &shardHash, sizeof(shardHash));

      SHLOG_TRACE("Hashing input {}", input);
      hash_update(input, &hashState);

      const auto params = blk->parameters(blk);
      for (uint32_t nParam = 0; nParam < params.len; nParam++) {
        const auto param = blk->getParam(blk, nParam);
        SHLOG_TRACE("Hashing param {}", param);
        hash_update(param, &hashState);
      }

      output = activateShard(blk, context, input);
      SHLOG_TRACE("Hashing output {}", output);
      hash_update(output, &hashState);
    } else {
      output = activateShard(blk, context, input);
    }

    // Deal with aftermath of activation
    if (unlikely(!context->shouldContinue())) {
      auto state = context->getState();
      switch (state) {
      case SHWireState::Return:
        if constexpr (HANDLES_RETURN)
          context->continueFlow();
        return SHWireState::Return;
      case SHWireState::Error:
        SHLOG_ERROR("Shard activation error, failed shard: {}, error: {}", blk->name(blk), context->getErrorMessage());
      case SHWireState::Stop:
      case SHWireState::Restart:
        return state;
      case SHWireState::Rebase:
        // reset input to wire one
        // and reset state
        input = wireInput;
        context->continueFlow();
        continue;
      case SHWireState::Continue:
        break;
      }
    }

    // Pass output to next block input
    input = output;
  }
  return SHWireState::Continue;
}

SHWireState activateShards(Shards shards, SHContext *context, const SHVar &wireInput, SHVar &output) noexcept {
  return shardsActivation<Shards, false, false>(shards, context, wireInput, output);
}

SHWireState activateShards2(Shards shards, SHContext *context, const SHVar &wireInput, SHVar &output) noexcept {
  return shardsActivation<Shards, true, false>(shards, context, wireInput, output);
}

SHWireState activateShards(SHSeq shards, SHContext *context, const SHVar &wireInput, SHVar &output) noexcept {
  return shardsActivation<SHSeq, false, false>(shards, context, wireInput, output);
}

SHWireState activateShards2(SHSeq shards, SHContext *context, const SHVar &wireInput, SHVar &output) noexcept {
  return shardsActivation<SHSeq, true, false>(shards, context, wireInput, output);
}

SHWireState activateShards(Shards shards, SHContext *context, const SHVar &wireInput, SHVar &output, SHVar &outHash) noexcept {
  return shardsActivation<Shards, false, true>(shards, context, wireInput, output, &outHash);
}

SHWireState activateShards2(Shards shards, SHContext *context, const SHVar &wireInput, SHVar &output, SHVar &outHash) noexcept {
  return shardsActivation<Shards, true, true>(shards, context, wireInput, output, &outHash);
}

SHRunWireOutput runWire(SHWire *wire, SHContext *context, const SHVar &wireInput) {
  ZoneScoped;
  ZoneName(wire->name.c_str(), wire->name.size());

  memset(&wire->previousOutput, 0x0, sizeof(SHVar));
  wire->currentInput = wireInput;
  wire->state = SHWire::State::Iterating;
  wire->context = context;
  DEFER({ wire->state = SHWire::State::IterationEnded; });

  wire->dispatcher.trigger(SHWire::OnUpdateEvent{wire});

  try {
    auto state =
        shardsActivation<std::vector<ShardPtr>, false, false>(wire->shards, context, wire->currentInput, wire->previousOutput);
    switch (state) {
    case SHWireState::Return:
      return {context->getFlowStorage(), SHRunWireOutputState::Stopped};
    case SHWireState::Restart:
      return {context->getFlowStorage(), SHRunWireOutputState::Restarted};
    case SHWireState::Error:
      // shardsActivation handles error logging and such
      assert(context->failed());
      return {wire->previousOutput, SHRunWireOutputState::Failed};
    case SHWireState::Stop:
      assert(!context->failed());
      return {context->getFlowStorage(), SHRunWireOutputState::Stopped};
    case SHWireState::Rebase:
      // Handled inside shardsActivation
      SHLOG_FATAL("Invalid wire state");
    case SHWireState::Continue:
      break;
    }
  }
#ifndef __EMSCRIPTEN__
  catch (boost::context::detail::forced_unwind const &e) {
    throw; // required for Boost Coroutine!
  }
#endif
  catch (...) {
    // shardsActivation handles error logging and such
    return {wire->previousOutput, SHRunWireOutputState::Failed};
  }

  return {wire->previousOutput, SHRunWireOutputState::Running};
}

} // namespace shards
