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
SH_HAS_MEMBER_TEST(composeV2);
SH_HAS_MEMBER_TEST(parameters);
SH_HAS_MEMBER_TEST(setParam);
SH_HAS_MEMBER_TEST(getParam);
SH_HAS_MEMBER_TEST(warmup);
SH_HAS_MEMBER_TEST(activate);
SH_HAS_MEMBER_TEST(cleanup);
SH_HAS_MEMBER_TEST(mutate);
SH_HAS_MEMBER_TEST(crossover);
SH_HAS_MEMBER_TEST(getState);
SH_HAS_MEMBER_TEST(setState);
SH_HAS_MEMBER_TEST(resetState);

template <class T> struct ShardStaticWrapper {
  ShardStaticInterface header{};

  static inline const char *name = "";
  static inline const char *aliasOf = "";
  static inline uint32_t crc = 0;

  static ShardMetadata &metadata() { return get()->metadata; }

  ShardStaticWrapper();
  static __cdecl ShardStaticInterface *get() {
    static ShardStaticWrapper instance;
    return &instance.header;
  }
  static constexpr size_t size = sizeof(std::string);
};

// Composition is preferred
template <class T> struct ShardWrapper {
  Shard header;
  T shard;
  std::string lastError;
  SHVar outputStorage; // we added this as a refactor workaround, when activate returns a non ref/pointer type
};

namespace wrapper_detail {
template <typename T> T &shard(Shard *b) { return reinterpret_cast<ShardWrapper<T> *>(b)->shard; }
template <typename T> ShardWrapper<T> &wrapper(Shard *b) { return *reinterpret_cast<ShardWrapper<T> *>(b); }
} // namespace wrapper_detail
template <typename T> ShardStaticWrapper<T>::ShardStaticWrapper() {
  using namespace wrapper_detail;
  ShardStaticInterface *result = &header;

  result->create = static_cast<SHCreateProc>([](ShardStaticInterface *iface) -> Shard * {
    auto self = new (std::align_val_t{16}) ShardWrapper<T>();
    Shard &shard = self->header;
    shard.iface = iface;
    shard.activate = shard.iface->activate;
    return &shard;
  });

  // name
  if constexpr (has_name<T>::value) {
    result->name = static_cast<SHNameProc>([](Shard *b) { return shard<T>(b).name(); });
  } else {
    result->name = static_cast<SHNameProc>([](Shard *b) { return name; });
  }
  result->nameLength = strlen(result->name(nullptr));

  // hash
  if constexpr (has_hash<T>::value) {
    result->hash = static_cast<SHHashProc>([](Shard *b) { return shard<T>(b).hash(); });
  } else {
    result->hash = static_cast<SHHashProc>([](Shard *b) { return crc; });
  }

  // help
  if constexpr (has_help<T>::value) {
    result->help = static_cast<SHHelpProc>([](Shard *b) { return shard<T>(b).help(); });
  } else {
    result->help = static_cast<SHHelpProc>([](Shard *b) { return SHOptionalString(); });
  }

  // inputHelp
  if constexpr (has_inputHelp<T>::value) {
    result->inputHelp = static_cast<SHHelpProc>([](Shard *b) { return shard<T>(b).inputHelp(); });
  } else {
    result->inputHelp = static_cast<SHHelpProc>([](Shard *b) { return SHOptionalString(); });
  }

  // outputHelp
  if constexpr (has_outputHelp<T>::value) {
    result->outputHelp = static_cast<SHHelpProc>([](Shard *b) { return shard<T>(b).outputHelp(); });
  } else {
    result->outputHelp = static_cast<SHHelpProc>([](Shard *b) { return SHOptionalString(); });
  }

  // properties
  if constexpr (has_properties<T>::value) {
    result->properties = static_cast<SHPropertiesProc>([](Shard *b) -> const SHTable * { return shard<T>(b).properties(); });
  } else {
    result->properties = static_cast<SHPropertiesProc>([](Shard *b) -> const SHTable * { return nullptr; });
  }

  // setup
  if constexpr (has_setup<T>::value) {
    result->setup = static_cast<SHSetupProc>([](Shard *b) { shard<T>(b).setup(); });
  } else {
    result->setup = static_cast<SHSetupProc>([](Shard *b) {});
  }

  // destroy
  if constexpr (has_destroy<T>::value) {
    result->destroy = static_cast<SHDestroyProc>([](Shard *b) {
      auto &w = wrapper<T>(b);
      w.shard.destroy();
      w.~ShardWrapper<T>();
      ::operator delete(&w, std::align_val_t{16});
    });
  } else {
    result->destroy = static_cast<SHDestroyProc>([](Shard *b) {
      auto &w = wrapper<T>(b);
      w.~ShardWrapper<T>();
      ::operator delete(&w, std::align_val_t{16});
    });
  }

  // inputTypes
  static_assert(has_inputTypes<T>::value, "Shards must have an \"inputTypes\" method.");
  if constexpr (has_inputTypes<T>::value) {
    result->inputTypes = static_cast<SHInputTypesProc>([](Shard *b) { return shard<T>(b).inputTypes(); });
  }

  // outputTypes
  static_assert(has_outputTypes<T>::value, "Shards must have an \"outputTypes\" method.");
  if constexpr (has_outputTypes<T>::value) {
    result->outputTypes = static_cast<SHOutputTypesProc>([](Shard *b) { return shard<T>(b).outputTypes(); });
  }

  // exposedVariables
  if constexpr (has_exposedVariables<T>::value) {
    result->exposedVariables = static_cast<SHExposedVariablesProc>([](Shard *b) { return shard<T>(b).exposedVariables(); });
  } else {
    result->exposedVariables = static_cast<SHExposedVariablesProc>([](Shard *b) { return SHExposedTypesInfo(); });
  }

  // requiredVariables
  if constexpr (has_requiredVariables<T>::value) {
    result->requiredVariables = static_cast<SHRequiredVariablesProc>([](Shard *b) { return shard<T>(b).requiredVariables(); });
  } else {
    result->requiredVariables = static_cast<SHRequiredVariablesProc>([](Shard *b) { return SHExposedTypesInfo(); });
  }

  // parameters
  if constexpr (has_parameters<T>::value) {
    result->parameters = static_cast<SHParametersProc>([](Shard *b) { return shard<T>(b).parameters(); });
  } else {
    result->parameters = static_cast<SHParametersProc>([](Shard *b) { return SHParametersInfo(); });
  }

  // setParam
  if constexpr (has_setParam<T>::value) {
    result->setParam = static_cast<SHSetParamProc>([](Shard *b, int i, const SHVar *v) {
      try {
        shard<T>(b).setParam(i, *v);
        return SHError::Success;
      } catch (const std::exception &e) {
        auto &w = wrapper<T>(b);
        w.lastError.assign(e.what());
        return SHError{1, SHStringWithLen{w.lastError.data(), w.lastError.size()}};
      }
    });
  } else {
    result->setParam = static_cast<SHSetParamProc>([](Shard *b, int i, const SHVar *v) { return SHError::Success; });
  }

  // getParam
  if constexpr (has_getParam<T>::value) {
    result->getParam = static_cast<SHGetParamProc>([](Shard *b, int i) { return shard<T>(b).getParam(i); });
  } else {
    result->getParam = static_cast<SHGetParamProc>([](Shard *b, int i) { return SHVar(); });
  }

  // compose
  if constexpr (has_compose<T>::value) {
    result->compose = static_cast<SHComposeProc>([](Shard *b, SHInstanceData *data) {
      try {
        return SHShardComposeResult{SHError::Success, shard<T>(b).compose(*data)};
      } catch (std::exception &e) {
        auto &w = wrapper<T>(b);
        w.lastError.assign(e.what());
        return SHShardComposeResult{SHError{1, SHStringWithLen{reinterpret_cast<ShardWrapper<T> *>(b)->lastError.data(),
                                                               reinterpret_cast<ShardWrapper<T> *>(b)->lastError.size()}},
                                    SHTypeInfo{}};
      }
    });
  } else {
    // compose is optional!
    result->compose = nullptr;
  }

  // composeV2
  if constexpr (has_composeV2<T>::value) {
    result->composeV2 = static_cast<SHComposeV2Proc>([](Shard *b, SHInstanceData *data) {
      try {
        return SHShardComposeResult{SHError::Success, reinterpret_cast<ShardWrapper<T> *>(b)->shard.composeV2(*data)};
      } catch (std::exception &e) {
        auto &w = wrapper<T>(b);
        w.lastError.assign(e.what());
        return SHShardComposeResult{SHError{1, SHStringWithLen{w.lastError.data(), w.lastError.size()}}, SHTypeInfo{}};
      }
    });
  } else {
    // composeV2 is optional!
    result->composeV2 = nullptr;
  }

  // warmup
  if constexpr (has_warmup<T>::value) {
    result->warmup = static_cast<SHWarmupProc>([](Shard *b, SHContext *ctx) {
      try {
        shard<T>(b).warmup(ctx);
        return SHError::Success;
      } catch (const std::exception &e) {
        auto &w = wrapper<T>(b);
        w.lastError.assign(e.what());
        return SHError{1, SHStringWithLen{w.lastError.data(), w.lastError.size()}};
      }
    });
  } else {
    // warmup is optional!
    result->warmup = nullptr;
  }

  // activate
  static_assert(has_activate<T>::value, "Shards must have an \"activate\" method.");
  if constexpr (has_activate<T>::value) {
    result->activate = static_cast<SHActivateProc>([](Shard *b, SHContext *ctx, const SHVar *v) -> const SHVar * {
      auto &w = wrapper<T>(b);
      try {
        using ReturnType = decltype(w.shard.activate(ctx, *v));
        if constexpr (std::is_same_v<ReturnType, void>) {
          w.shard.activate(ctx, *v);
          return v; // Return the input if activate doesn't return anything
        } else if constexpr (std::is_reference_v<ReturnType>) {
          return &w.shard.activate(ctx, *v); // Return the reference directly
        } else {
          // Return a shallow copy, this technically is a deprecated behavior which should be slowly removed
          w.outputStorage = w.shard.activate(ctx, *v);
          return &w.outputStorage;
        }
      } catch (const std::exception &e) {
        shards::abortWire(ctx, e.what());
        return &w.outputStorage;
      }
    });
  }

  // cleanup
  if constexpr (has_cleanup<T>::value) {
    result->cleanup = static_cast<SHCleanupProc>([](Shard *b, SHContext *context) {
      try {
        auto &w = wrapper<T>(b);
        if constexpr (std::is_invocable_v<decltype(&T::cleanup), T &, SHContext *>) {
          w.shard.cleanup(context);
        } else {
          w.shard.cleanup();
        }
        return SHError::Success;
      } catch (const std::exception &e) {
        auto &w = wrapper<T>(b);
        w.lastError.assign(e.what());
        return SHError{1, SHStringWithLen{w.lastError.data(), w.lastError.size()}};
      }
    });
  } else {
    result->cleanup = static_cast<SHCleanupProc>([](Shard *b, SHContext *) { return SHError::Success; });
  }

  // mutate
  if constexpr (has_mutate<T>::value) {
    result->mutate = static_cast<SHMutateProc>([](Shard *b, SHTable options) { shard<T>(b).mutate(options); });
  } else {
    // mutate is optional!
    result->mutate = nullptr;
  }

  // crossover
  if constexpr (has_crossover<T>::value) {
    result->crossover = static_cast<SHCrossoverProc>(
        [](Shard *b, const SHVar *state0, const SHVar *state1) { shard<T>(b).crossover(*state0, *state1); });
  } else {
    // crossover is optional!
    result->crossover = nullptr;
  }

  // getState
  if constexpr (has_getState<T>::value) {
    result->getState = static_cast<SHGetStateProc>([](Shard *b) { return shard<T>(b).getState(); });
  } else {
    // getState is optional!
    result->getState = nullptr;
  }

  // setState
  if constexpr (has_setState<T>::value) {
    result->setState = static_cast<SHSetStateProc>([](Shard *b, const SHVar *state) { shard<T>(b).setState(*state); });
  } else {
    // setState is optional!
    result->setState = nullptr;
  }

  // resetState
  if constexpr (has_resetState<T>::value) {
    result->resetState = static_cast<SHResetStateProc>([](Shard *b) { shard<T>(b).resetState(); });
  } else {
    // resetState is optional!
    result->resetState = nullptr;
  }
}

#ifdef SHARDS_THIS_MODULE_ID
#define SHARD_MODULE_STRINGIFY_HELPER(x) #x
#define SHARD_MODULE_STRINGIFY(x) SHARD_MODULE_STRINGIFY_HELPER(x)
#else
#define SHARD_MODULE_STRINGIFY(x) ""
#endif

#define REGISTER_SHARD(__name__, __type__)                                                                                   \
  ::shards::ShardStaticWrapper<__type__>::name = __name__;                                                                   \
  ::shards::ShardStaticWrapper<__type__>::crc = ::shards::constant<::shards::crc32(__name__ SHARDS_CURRENT_ABI_STR)>::value; \
  ::shards::ShardStaticWrapper<__type__>::metadata().category = SHString(SHARD_MODULE_STRINGIFY(SHARDS_THIS_MODULE_ID));     \
  ::shards::registerShard(::shards::ShardStaticWrapper<__type__>::name, ::shards::ShardStaticWrapper<__type__>::get(),           \
                          NAMEOF_FULL_TYPE(__type__))

#define REGISTER_SHARD_ALIAS(__name__, __aliasOf__, __type__)               \
  ::shards::ShardStaticWrapper<__type__>::metadata().aliasOf = __aliasOf__; \
  ::shards::registerShard(__name__, ::shards::ShardStaticWrapper<__type__>::get(), NAMEOF_FULL_TYPE(__type__))

#define OVERRIDE_ACTIVATE(__data__, __func__)                                                                            \
  __data__.shard->activate = static_cast<SHActivateProc>([](Shard *b, SHContext *ctx, const SHVar *v) -> const SHVar * { \
    auto self = reinterpret_cast<shards::ShardWrapper<typename std::remove_pointer<decltype(this)>::type> *>(b);         \
    try {                                                                                                                \
      self->outputStorage = self->shard.__func__(ctx, *v);                                                               \
      return &self->outputStorage;                                                                                       \
    } catch (std::exception & e) {                                                                                       \
      shards::abortWire(ctx, e.what());                                                                                  \
      return &self->outputStorage;                                                                                       \
    }                                                                                                                    \
  })

#define OVERRIDE_ACTIVATE1(__data__, __func__)                                                                           \
  __data__.shard->activate = static_cast<SHActivateProc>([](Shard *b, SHContext *ctx, const SHVar *v) -> const SHVar * { \
    auto self = reinterpret_cast<shards::ShardWrapper<typename std::remove_pointer<decltype(this)>::type> *>(b);         \
    try {                                                                                                                \
      self->shard.__func__(ctx, *v);                                                                                     \
      return v;                                                                                                          \
    } catch (std::exception & e) {                                                                                       \
      shards::abortWire(ctx, e.what());                                                                                  \
      return &self->outputStorage;                                                                                       \
    }                                                                                                                    \
  })

template <typename SHCORE, Parameters &Params, size_t NPARAMS, Type &InputType, Type &OutputType> struct TSimpleShard {
  static SHTypesInfo inputTypes() { return InputType; }
  static SHTypesInfo outputTypes() { return OutputType; }
  static SHParametersInfo parameters() { return Params; }

  void setParam(int index, const SHVar &value) { params[index] = value; }

  SHVar getParam(int index) { return params[index]; }

  void cleanup(SHContext *context) {
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
