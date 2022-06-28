#ifndef B0328A63_0B69_4191_94D5_38783B9F20C9
#define B0328A63_0B69_4191_94D5_38783B9F20C9

#include "stddef.h"
#include <foundation.hpp>
#include <type_traits>
#include "shardwrapper.hpp"

// Template helpers for setParam/getParam
namespace shards {

#define PARAM(_type, _name, _displayName, _help, _types)                                     \
  static inline ParameterInfo _name##ParameterInfo = {_displayName, SHCCSTR(_help), _types}; \
  _type _name;

#define PARAM_VAR(_name, _displayName, _help, ...) PARAM(Var, _name, _displayName, _help, __VA_ARGS__)
#define PARAM_PARAMVAR(_name, _displayName, _help, ...) PARAM(ParamVar, _name, _displayName, _help, __VA_ARGS__)

struct IterableParam {
  // Return address of param inside shard
  void *(*resolveParamInShard)(void *shardPtr);
  const ParameterInfo *paramInfo;

  void (*setParam)(void *varPtr, SHVar var){};
  SHVar (*getParam)(void *varPtr){};
  void (*warmup)(void *varPtr, SHContext *ctx){};
  void (*cleanup)(void *varPtr){};

  template <typename T> T &get(void *obj) const { return *(T *)resolveParamInShard(obj); }

  template <typename T> static IterableParam create(void *(*resolveParamInShard)(void *), const ParameterInfo *paramInfo) {
    return createWithVarInterface<T>(resolveParamInShard, paramInfo);
  }

  template <typename T>
  static IterableParam createWithVarInterface(void *(*resolveParamInShard)(void *), const ParameterInfo *paramInfo) {
    IterableParam result{
        .resolveParamInShard = resolveParamInShard,
        .paramInfo = paramInfo,
        .setParam = [](void *varPtr, SHVar var) { *((T *)varPtr) = var; },
        .getParam = [](void *varPtr) -> SHVar { return *((T *)varPtr); },
    };

    if constexpr (has_warmup<T>::value) {
      result.warmup = [](void *varPtr, SHContext *ctx) { ((T *)varPtr)->warmup(ctx); };
    } else {
      result.warmup = [](void *, SHContext *) {};
    }

    if constexpr (has_warmup<T>::value) {
      result.cleanup = [](void *varPtr) { ((T *)varPtr)->cleanup(); };
    } else {
      result.cleanup = [](void *) {};
    }

    return result;
  }
};

// Usage:
//  PARAM_IMPL(Shard,
//    PARAM_IMPL_FOR(Param0),
//    PARAM_IMPL_FOR(Param1),
//    PARAM_IMPL_FOR(Param2))
// Side effects:
//  - typedefs Self to the current class
//  - creates a static member named iterableParams
#define PARAM_IMPL(_type, ...)                                          \
  typedef _type Self;                                                   \
  static const IterableParam *getIterableParams(size_t &outNumParams) { \
    static IterableParam result[] = {__VA_ARGS__};                      \
    outNumParams = std::extent<decltype(result)>::value;                \
    return result;                                                      \
  }                                                                     \
  PARAM_PARAMS()                                                        \
  PARAM_GET_SET()

#define PARAM_IMPL_FOR(_name) \
  IterableParam::create<decltype(_name)>([](void *obj) -> void * { return (void *)&((Self *)obj)->_name; }, &_name##ParameterInfo)

// Implements parameters()
#define PARAM_PARAMS()                                                          \
  static SHParametersInfo parameters() {                                        \
    static SHParametersInfo result = []() {                                     \
      SHParametersInfo result{};                                                \
      size_t numParams;                                                         \
      const IterableParam *params = getIterableParams(numParams);               \
      arrayResize(result, numParams);                                           \
      for (size_t i = 0; i < numParams; i++) {                                  \
        result.elements[i] = *const_cast<ParameterInfo *>(params[i].paramInfo); \
      }                                                                         \
      return result;                                                            \
    }();                                                                        \
    return result;                                                              \
  }

// Implements setParam()/getParam()
#define PARAM_GET_SET()                                                       \
  void setParam(int index, const SHVar &value) {                              \
    size_t numParams;                                                         \
    const IterableParam *params = getIterableParams(numParams);               \
    if (index < numParams) {                                                  \
      params[index].setParam(params[index].resolveParamInShard(this), value); \
    } else {                                                                  \
      throw InvalidParameterIndex();                                          \
    }                                                                         \
  }                                                                           \
  SHVar getParam(int index) {                                                 \
    size_t numParams;                                                         \
    const IterableParam *params = getIterableParams(numParams);               \
    if (index < numParams) {                                                  \
      return params[index].getParam(params[index].resolveParamInShard(this)); \
    } else {                                                                  \
      throw InvalidParameterIndex();                                          \
    }                                                                         \
  }

// Implements warmup(ctx) for parameters
// call from warmup manually with context
#define PARAM_WARMUP(_ctx)                                         \
  {                                                                \
    size_t numParams;                                              \
    const IterableParam *params = getIterableParams(numParams);    \
    for (size_t i = 0; i < numParams; i++)                         \
      params[i].warmup(params[i].resolveParamInShard(this), _ctx); \
  }

// implements cleanup() for parameters
// call from cleanup manually
#define PARAM_CLEANUP()                                         \
  {                                                             \
    size_t numParams;                                           \
    const IterableParam *params = getIterableParams(numParams); \
    for (size_t i = numParams - 1;; i--) {                      \
      params[i].cleanup(params[i].resolveParamInShard(this));   \
      if (i == 0)                                               \
        break;                                                  \
    }                                                           \
  }

} // namespace shards

#endif /* B0328A63_0B69_4191_94D5_38783B9F20C9 */
