/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2019 Fragcolor Pte. Ltd. */

#ifndef SH_SHARDWRAPPER_HPP
#define SH_SHARDWRAPPER_HPP

#include "shards.hpp"
#include "utility.hpp"

namespace shards {
SH_HAS_MEMBER_TEST(name);
SH_HAS_MEMBER_TEST(hash);
SH_HAS_MEMBER_TEST(help);
SH_HAS_MEMBER_TEST(inputHelp);
SH_HAS_MEMBER_TEST(outputHelp);
SH_HAS_MEMBER_TEST(properties);
SH_HAS_MEMBER_TEST(setup);
SH_HAS_MEMBER_TEST(destroy);
SH_HAS_MEMBER_TEST(inputTypes);
SH_HAS_MEMBER_TEST(outputTypes);
SH_HAS_MEMBER_TEST(exposedVariables);
SH_HAS_MEMBER_TEST(requiredVariables);
SH_HAS_MEMBER_TEST(compose);
SH_HAS_MEMBER_TEST(composed);
SH_HAS_MEMBER_TEST(parameters);
SH_HAS_MEMBER_TEST(setParam);
SH_HAS_MEMBER_TEST(getParam);
SH_HAS_MEMBER_TEST(warmup);
SH_HAS_MEMBER_TEST(activate);
SH_HAS_MEMBER_TEST(nextFrame);
SH_HAS_MEMBER_TEST(cleanup);
SH_HAS_MEMBER_TEST(mutate);
SH_HAS_MEMBER_TEST(crossover);
SH_HAS_MEMBER_TEST(getState);
SH_HAS_MEMBER_TEST(setState);
SH_HAS_MEMBER_TEST(resetState);

// Composition is preferred
template <class T> struct ShardWrapper {
  Shard header;
  T shard;
  std::string lastError;
  static inline const char *name = "";
  static inline uint32_t crc = 0;

  static __cdecl Shard *create() {
    Shard *result = reinterpret_cast<Shard *>(new (std::align_val_t{16}) ShardWrapper<T>());

    // name
    if constexpr (has_name<T>::value) {
      result->name = static_cast<SHNameProc>([](Shard *b) { return reinterpret_cast<ShardWrapper<T> *>(b)->shard.name(); });
    } else {
      result->name = static_cast<SHNameProc>([](Shard *b) { return name; });
    }

    // hash
    if constexpr (has_hash<T>::value) {
      result->hash = static_cast<SHHashProc>([](Shard *b) { return reinterpret_cast<ShardWrapper<T> *>(b)->shard.hash(); });
    } else {
      result->hash = static_cast<SHHashProc>([](Shard *b) { return crc; });
    }

    // help
    if constexpr (has_help<T>::value) {
      result->help = static_cast<SHHelpProc>([](Shard *b) { return reinterpret_cast<ShardWrapper<T> *>(b)->shard.help(); });
    } else {
      result->help = static_cast<SHHelpProc>([](Shard *b) { return SHOptionalString(); });
    }

    // inputHelp
    if constexpr (has_inputHelp<T>::value) {
      result->inputHelp =
          static_cast<SHHelpProc>([](Shard *b) { return reinterpret_cast<ShardWrapper<T> *>(b)->shard.inputHelp(); });
    } else {
      result->inputHelp = static_cast<SHHelpProc>([](Shard *b) { return SHOptionalString(); });
    }

    // outputHelp
    if constexpr (has_outputHelp<T>::value) {
      result->outputHelp =
          static_cast<SHHelpProc>([](Shard *b) { return reinterpret_cast<ShardWrapper<T> *>(b)->shard.outputHelp(); });
    } else {
      result->outputHelp = static_cast<SHHelpProc>([](Shard *b) { return SHOptionalString(); });
    }

    // properties
    if constexpr (has_properties<T>::value) {
      result->properties = static_cast<SHPropertiesProc>(
          [](Shard *b) -> const SHTable * { return reinterpret_cast<ShardWrapper<T> *>(b)->shard.properties(); });
    } else {
      result->properties = static_cast<SHPropertiesProc>([](Shard *b) -> const SHTable * { return nullptr; });
    }

    // setup
    if constexpr (has_setup<T>::value) {
      result->setup = static_cast<SHSetupProc>([](Shard *b) { reinterpret_cast<ShardWrapper<T> *>(b)->shard.setup(); });
    } else {
      result->setup = static_cast<SHSetupProc>([](Shard *b) {});
    }

    // destroy
    if constexpr (has_destroy<T>::value) {
      result->destroy = static_cast<SHDestroyProc>([](Shard *b) {
        auto bw = reinterpret_cast<ShardWrapper<T> *>(b);
        bw->shard.destroy();
        bw->ShardWrapper<T>::~ShardWrapper<T>();
        ::operator delete (bw, std::align_val_t{16});
      });
    } else {
      result->destroy = static_cast<SHDestroyProc>([](Shard *b) {
        auto bw = reinterpret_cast<ShardWrapper<T> *>(b);
        bw->ShardWrapper<T>::~ShardWrapper<T>();
        ::operator delete (bw, std::align_val_t{16});
      });
    }

    // inputTypes
    static_assert(has_inputTypes<T>::value, "Shards must have an \"inputTypes\" method.");
    if constexpr (has_inputTypes<T>::value) {
      result->inputTypes =
          static_cast<SHInputTypesProc>([](Shard *b) { return reinterpret_cast<ShardWrapper<T> *>(b)->shard.inputTypes(); });
    }

    // outputTypes
    static_assert(has_outputTypes<T>::value, "Shards must have an \"outputTypes\" method.");
    if constexpr (has_outputTypes<T>::value) {
      result->outputTypes =
          static_cast<SHOutputTypesProc>([](Shard *b) { return reinterpret_cast<ShardWrapper<T> *>(b)->shard.outputTypes(); });
    }

    // exposedVariables
    if constexpr (has_exposedVariables<T>::value) {
      result->exposedVariables = static_cast<SHExposedVariablesProc>(
          [](Shard *b) { return reinterpret_cast<ShardWrapper<T> *>(b)->shard.exposedVariables(); });
    } else {
      result->exposedVariables = static_cast<SHExposedVariablesProc>([](Shard *b) { return SHExposedTypesInfo(); });
    }

    // requiredVariables
    if constexpr (has_requiredVariables<T>::value) {
      result->requiredVariables = static_cast<SHRequiredVariablesProc>(
          [](Shard *b) { return reinterpret_cast<ShardWrapper<T> *>(b)->shard.requiredVariables(); });
    } else {
      result->requiredVariables = static_cast<SHRequiredVariablesProc>([](Shard *b) { return SHExposedTypesInfo(); });
    }

    // parameters
    if constexpr (has_parameters<T>::value) {
      result->parameters =
          static_cast<SHParametersProc>([](Shard *b) { return reinterpret_cast<ShardWrapper<T> *>(b)->shard.parameters(); });
    } else {
      result->parameters = static_cast<SHParametersProc>([](Shard *b) { return SHParametersInfo(); });
    }

    // setParam
    if constexpr (has_setParam<T>::value) {
      result->setParam = static_cast<SHSetParamProc>([](Shard *b, int i, const SHVar *v) {
        try {
          reinterpret_cast<ShardWrapper<T> *>(b)->shard.setParam(i, *v);
          return SHError{0};
        } catch (const std::exception &e) {
          reinterpret_cast<ShardWrapper<T> *>(b)->lastError.assign(e.what());
          return SHError{1, reinterpret_cast<ShardWrapper<T> *>(b)->lastError.c_str()};
        }
      });
    } else {
      result->setParam = static_cast<SHSetParamProc>([](Shard *b, int i, const SHVar *v) { return SHError{0}; });
    }

    // getParam
    if constexpr (has_getParam<T>::value) {
      result->getParam =
          static_cast<SHGetParamProc>([](Shard *b, int i) { return reinterpret_cast<ShardWrapper<T> *>(b)->shard.getParam(i); });
    } else {
      result->getParam = static_cast<SHGetParamProc>([](Shard *b, int i) { return SHVar(); });
    }

    // compose
    if constexpr (has_compose<T>::value) {
      result->compose = static_cast<SHComposeProc>([](Shard *b, SHInstanceData data) {
        try {
          return SHShardComposeResult{SHError{0}, reinterpret_cast<ShardWrapper<T> *>(b)->shard.compose(data)};
        } catch (std::exception &e) {
          reinterpret_cast<ShardWrapper<T> *>(b)->lastError.assign(e.what());
          return SHShardComposeResult{SHError{1, reinterpret_cast<ShardWrapper<T> *>(b)->lastError.c_str()}, SHTypeInfo{}};
        }
      });
    } else {
      // compose is optional!
      result->compose = nullptr;
    }

    // composed
    if constexpr (has_composed<T>::value) {
      result->composed = static_cast<SHComposedProc>([](Shard *b, const SHWire *wire, const SHComposeResult *result) {
        reinterpret_cast<ShardWrapper<T> *>(b)->shard.composed(wire, result);
      });
    } else {
      // composed is optional!
      result->composed = nullptr;
    }

    // warmup
    if constexpr (has_warmup<T>::value) {
      result->warmup = static_cast<SHWarmupProc>([](Shard *b, SHContext *ctx) {
        try {
          reinterpret_cast<ShardWrapper<T> *>(b)->shard.warmup(ctx);
          return SHError{0};
        } catch (const std::exception &e) {
          reinterpret_cast<ShardWrapper<T> *>(b)->lastError.assign(e.what());
          return SHError{1, reinterpret_cast<ShardWrapper<T> *>(b)->lastError.c_str()};
        }
      });
    } else {
      // warmup is optional!
      result->warmup = nullptr;
    }

    // nextFrame
    if constexpr (has_nextFrame<T>::value) {
      result->nextFrame = static_cast<SHNextFrameProc>(
          [](Shard *b, SHContext *ctx) { reinterpret_cast<ShardWrapper<T> *>(b)->shard.nextFrame(ctx); });
    } else {
      // nextFrame is optional!
      result->nextFrame = nullptr;
    }

    // activate
    static_assert(has_activate<T>::value, "Shards must have an \"activate\" method.");
    if constexpr (has_activate<T>::value) {
      result->activate = static_cast<SHActivateProc>([](Shard *b, SHContext *ctx, const SHVar *v) {
        return reinterpret_cast<ShardWrapper<T> *>(b)->shard.activate(ctx, *v);
      });
    }

    // cleanup
    if constexpr (has_cleanup<T>::value) {
      result->cleanup = static_cast<SHCleanupProc>([](Shard *b) {
        try {
          reinterpret_cast<ShardWrapper<T> *>(b)->shard.cleanup();
          return SHError{0};
        } catch (const std::exception &e) {
          reinterpret_cast<ShardWrapper<T> *>(b)->lastError.assign(e.what());
          return SHError{1, reinterpret_cast<ShardWrapper<T> *>(b)->lastError.c_str()};
        }
      });
    } else {
      result->cleanup = static_cast<SHCleanupProc>([](Shard *b) { return SHError{0}; });
    }

    // mutate
    if constexpr (has_mutate<T>::value) {
      result->mutate = static_cast<SHMutateProc>(
          [](Shard *b, SHTable options) { reinterpret_cast<ShardWrapper<T> *>(b)->shard.mutate(options); });
    } else {
      // mutate is optional!
      result->mutate = nullptr;
    }

    // crossover
    if constexpr (has_crossover<T>::value) {
      result->crossover = static_cast<SHCrossoverProc>([](Shard *b, const SHVar *state0, const SHVar *state1) {
        reinterpret_cast<ShardWrapper<T> *>(b)->shard.crossover(*state0, *state1);
      });
    } else {
      // crossover is optional!
      result->crossover = nullptr;
    }

    // getState
    if constexpr (has_getState<T>::value) {
      result->getState =
          static_cast<SHGetStateProc>([](Shard *b) { return reinterpret_cast<ShardWrapper<T> *>(b)->shard.getState(); });
    } else {
      // getState is optional!
      result->getState = nullptr;
    }

    // setState
    if constexpr (has_setState<T>::value) {
      result->setState = static_cast<SHSetStateProc>(
          [](Shard *b, const SHVar *state) { reinterpret_cast<ShardWrapper<T> *>(b)->shard.setState(*state); });
    } else {
      // setState is optional!
      result->setState = nullptr;
    }

    // resetState
    if constexpr (has_resetState<T>::value) {
      result->resetState =
          static_cast<SHResetStateProc>([](Shard *b) { reinterpret_cast<ShardWrapper<T> *>(b)->shard.resetState(); });
    } else {
      // resetState is optional!
      result->resetState = nullptr;
    }

    return result;
  }
};

#define REGISTER_SHARD(__name__, __type__)                                                                             \
  ::shards::ShardWrapper<__type__>::name = __name__;                                                                   \
  ::shards::ShardWrapper<__type__>::crc = ::shards::constant<::shards::crc32(__name__ SHARDS_CURRENT_ABI_STR)>::value; \
  ::shards::registerShard(::shards::ShardWrapper<__type__>::name, &::shards::ShardWrapper<__type__>::create,           \
                          NAMEOF_FULL_TYPE(__type__))

#define OVERRIDE_ACTIVATE(__data__, __func__)                                                                                \
  __data__.shard->activate = static_cast<SHActivateProc>([](Shard *b, SHContext *ctx, const SHVar *v) {                      \
    return reinterpret_cast<ShardWrapper<typename std::remove_pointer<decltype(this)>::type> *>(b)->shard.__func__(ctx, *v); \
  })

template <typename SHCORE, Parameters &Params, size_t NPARAMS, Type &InputType, Type &OutputType> struct TSimpleShard {
  static SHTypesInfo inputTypes() { return InputType; }
  static SHTypesInfo outputTypes() { return OutputType; }
  static SHParametersInfo parameters() { return Params; }

  void setParam(int index, const SHVar &value) { params[index] = value; }

  SHVar getParam(int index) { return params[index]; }

  void cleanup() {
    for (auto &param : params) {
      params.cleanup();
    }
  }

  void warmup(SHContext *context) {
    for (auto &param : params) {
      params.warmup(context);
    }
  }

protected:
  constexpr SHVar &param(size_t idx) {
    static_assert(idx < NPARAMS, "Parameter index out of range.");
    return params[idx].get();
  }

private:
  std::array<TParamVar<SHCORE>, NPARAMS> params;
};

typedef SHVar (*LambdaActivate)(const SHVar &input);
template <LambdaActivate F, Type &InputType, Type &OutputType> struct LambdaShard {
  static SHTypesInfo inputTypes() { return InputType; }
  static SHTypesInfo outputTypes() { return OutputType; }

  SHVar activate(SHContext *context, const SHVar &input) { return F(input); }
};
}; // namespace shards

#endif
