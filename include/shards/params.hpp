#ifndef SH_DLL_PARAMS_HPP
#define SH_DLL_PARAMS_HPP

#include "self_macro.h"
#include "shards.hpp"
#include "utility.hpp"
#include <stddef.h>
#include <vector>
#include <type_traits>
#include <shards/shardwrapper.hpp>

// Shards parameter macros for DLL context
// These provide the same PARAM_ macro API as shards/core/params.hpp
// but work without internal headers (foundation.hpp, etc.)
//
// Usage is identical to internal shards:
//   struct MyShard {
//     PARAM_PARAMVAR(_varName, "PublicName", "Help text", {::shards::CoreInfo::StringType})
//     PARAM_IMPL(PARAM_IMPL_FOR(_varName))
//
//     PARAM_REQUIRED_VARIABLES();
//     SHTypeInfo compose(SHInstanceData &data) {
//       PARAM_COMPOSE_REQUIRED_VARIABLES(data);
//       return outputTypes().elements[0];
//     }
//     void warmup(SHContext *ctx) { PARAM_WARMUP(ctx); }
//     void cleanup(SHContext *ctx) { PARAM_CLEANUP(ctx); }
//   };

namespace shards {

// Check if a type info contains context variables (also defined in exposed_type_utils.hpp)
// Uses a dedicated guard so either header can be included first without redefinition
#ifndef SH_HAS_CONTEXT_VARIABLES_DEFINED
#define SH_HAS_CONTEXT_VARIABLES_DEFINED
inline bool hasContextVariables(const SHTypeInfo &type) {
  switch (type.basicType) {
  case SHType::ContextVar:
    return true;
  case SHType::Seq:
    for (uint32_t i = 0; i < type.seqTypes.len; i++)
      if (hasContextVariables(type.seqTypes.elements[i]))
        return true;
    break;
  case SHType::Table:
    for (uint32_t i = 0; i < type.table.types.len; i++)
      if (hasContextVariables(type.table.types.elements[i]))
        return true;
    break;
  default:
    break;
  }
  return false;
}
#endif

// Templated ExposedInfo - uses SH_CORE for array operations
template <typename SH_CORE> struct TExposedInfo {
  SHExposedTypesInfo _innerInfo{};

  TExposedInfo() = default;

  TExposedInfo(const TExposedInfo &other) {
    for (uint32_t i = 0; i < other._innerInfo.len; i++) {
      push_back(other._innerInfo.elements[i]);
    }
  }

  TExposedInfo(TExposedInfo &&other) noexcept : _innerInfo(other._innerInfo) { other._innerInfo = {}; }

  TExposedInfo &operator=(const TExposedInfo &other) {
    if (this == &other)
      return *this;
    SH_CORE::expTypesResize(&_innerInfo, 0);
    for (uint32_t i = 0; i < other._innerInfo.len; i++) {
      push_back(other._innerInfo.elements[i]);
    }
    return *this;
  }

  TExposedInfo &operator=(TExposedInfo &&other) noexcept {
    if (this != &other) {
      SH_CORE::expTypesFree(&_innerInfo);
      _innerInfo = other._innerInfo;
      other._innerInfo = {};
    }
    return *this;
  }

  ~TExposedInfo() { SH_CORE::expTypesFree(&_innerInfo); }

  void push_back(const SHExposedTypeInfo &info) { SH_CORE::expTypesPush(&_innerInfo, &info); }

  void clear() { SH_CORE::expTypesResize(&_innerInfo, 0); }

  constexpr static SHExposedTypeInfo Variable(SHString name, SHOptionalString help, SHTypeInfo type, bool isMutable = false) {
    SHExposedTypeInfo res = {name, help, type, isMutable};
    return res;
  }

  constexpr static SHExposedTypeInfo ProtectedVariable(SHString name, SHOptionalString help, SHTypeInfo type,
                                                       bool isMutable = false) {
    SHExposedTypeInfo res = {name, help, type, isMutable, true};
    return res;
  }

  constexpr static SHExposedTypeInfo GlobalVariable(SHString name, SHOptionalString help, SHTypeInfo type,
                                                    bool isMutable = false) {
    SHExposedTypeInfo res = {name, help, type, isMutable, false, true};
    return res;
  }

  explicit operator SHExposedTypesInfo() const { return _innerInfo; }
};

// Check if an exposed variable's type matches any of the valid types for a parameter.
// validTypes entries with basicType==ContextVar contain the expected inner types in contextVarTypes.
// Uses SH_CORE::matchTypes (backed by TypeMatcher) for proper structural type matching.
template <typename SH_CORE>
inline bool matchesValidTypes(const SHTypeInfo &exposedType, SHTypesInfo validTypes) {
  for (uint32_t i = 0; i < validTypes.len; i++) {
    auto &vt = validTypes.elements[i];
    if (vt.basicType == SHType::Any)
      return true;
    if (vt.basicType == SHType::ContextVar) {
      if (vt.contextVarTypes.len == 0)
        return true; // No inner type constraints — accepts any context variable type
      for (uint32_t j = 0; j < vt.contextVarTypes.len; j++) {
        if (SH_CORE::matchTypes(exposedType, vt.contextVarTypes.elements[j], true, false))
          return true;
      }
    }
  }
  return false;
}

// Collect required context variables for DLL context
// Recursively walks Seq/Table values to find nested ContextVars,
// validates their types against validTypes, then registers them
template <typename SH_CORE>
inline void collectRequiredVariablesDll(const SHInstanceData &data, TExposedInfo<SH_CORE> &out, const SHVar &var,
                                        SHTypesInfo validTypes, const char *debugTag) {
  switch (var.valueType) {
  case SHType::ContextVar: {
    auto name = SHSTRVIEW(var);
    bool nameFound = false;
    for (uint32_t i = 0; i < data.shared.len; i++) {
      if (data.shared.elements[i].name && name == data.shared.elements[i].name) {
        nameFound = true;
        if (matchesValidTypes<SH_CORE>(data.shared.elements[i].exposedType, validTypes)) {
          out.push_back(data.shared.elements[i]);
          return;
        }
        // Type mismatch — continue scanning, data.shared may have duplicate names at different scopes
      }
    }
    if (nameFound) {
      std::string msg = "No matching variable found for parameter ";
      msg += debugTag;
      throw ::shards::SHException(msg);
    } else {
      std::string msg = "Required context variable '";
      msg += name;
      msg += "' not found for parameter ";
      msg += debugTag;
      throw ::shards::SHException(msg);
    }
  }
  case SHType::Seq: {
    auto &seq = var.payload.seqValue;
    for (uint32_t i = 0; i < seq.len; i++) {
      collectRequiredVariablesDll<SH_CORE>(data, out, seq.elements[i], validTypes, debugTag);
    }
    break;
  }
  case SHType::Table: {
    auto &t = var.payload.tableValue;
    SHTableIterator tit;
    t.api->tableGetIterator(t, &tit);
    SHVar k, v;
    while (t.api->tableNext(t, &tit, &k, &v)) {
      collectRequiredVariablesDll<SH_CORE>(data, out, v, validTypes, debugTag);
    }
    break;
  }
  default:
    break;
  }
}

// Only define macros if the internal params.hpp hasn't been included
#ifndef B0328A63_0B69_4191_94D5_38783B9F20C9

// SHCCSTR is defined in foundation.hpp (core-only); provide a plain fallback for DLL context
#ifndef SHCCSTR
#define SHCCSTR(_str_) (SHOptionalString{_str_, {}})
#endif

#define PARAM_EXT(_type, _name, _paramInfo)                                 \
  static inline ::shards::ParameterInfo &_name##ParameterInfo = _paramInfo; \
  _type _name{};

#define PARAM(_type, _name, _displayName, _help, ...)                                                       \
  static inline ::shards::ParameterInfo _name##ParameterInfo = {_displayName, SHCCSTR(_help), __VA_ARGS__}; \
  _type _name{};

#define PARAM_VAR(_name, _displayName, _help, ...) PARAM(::shards::OwnedVar, _name, _displayName, _help, __VA_ARGS__)

#define PARAM_PARAMVAR(_name, _displayName, _help, ...) PARAM(::shards::ParamVar, _name, _displayName, _help, __VA_ARGS__)

// Templated IterableParam for DLL context
template <typename SH_CORE> struct TIterableParam {
  void *(*resolveParamInShard)(void *shardPtr);
  const ::shards::ParameterInfo *paramInfo;

  void (*setParam)(void *varPtr, SHVar var){};
  SHVar (*getParam)(void *varPtr){};
  void (*collectRequirements)(const ::shards::TIterableParam<SH_CORE> &param, const SHInstanceData &data,
                              TExposedInfo<SH_CORE> &out, void *varPtr){};
  void (*warmup)(void *varPtr, SHContext *ctx){};
  void (*cleanup)(void *varPtr, SHContext *ctx){};

  template <typename T> T &get(void *obj) const { return *(T *)resolveParamInShard(obj); }

  template <typename T>
  static TIterableParam create(void *(*resolveParamInShard)(void *), const ParameterInfo *paramInfo) {
    TIterableParam result{
        .resolveParamInShard = resolveParamInShard,
        .paramInfo = paramInfo,
        .setParam = [](void *varPtr, SHVar var) { *((T *)varPtr) = var; },
        .getParam = [](void *varPtr) -> SHVar { return *((T *)varPtr); },
        .collectRequirements =
            [](const ::shards::TIterableParam<SH_CORE> &param, const SHInstanceData &data, TExposedInfo<SH_CORE> &out,
               void *varPtr) {
              collectRequiredVariablesDll<SH_CORE>(data, out, *((T *)varPtr), SHTypesInfo(param.paramInfo->_types),
                                                   param.paramInfo->_name);
            }};

    bool canPossiblyHaveContextVariables = false;
    for (auto &type : paramInfo->_types._types) {
      if (hasContextVariables(type)) {
        canPossiblyHaveContextVariables = true;
        break;
      }
    }
    if (!canPossiblyHaveContextVariables) {
      result.collectRequirements = nullptr;
    }

    if constexpr (has_warmup<T>::value) {
      result.warmup = [](void *varPtr, SHContext *ctx) { ((T *)varPtr)->warmup(ctx); };
    } else {
      result.warmup = [](void *, SHContext *) {};
    }

    if constexpr (has_cleanup<T>::value) {
      result.cleanup = [](void *varPtr, SHContext *ctx) { ((T *)varPtr)->cleanup(ctx); };
    } else {
      result.cleanup = [](void *, SHContext *ctx) {};
    }

    return result;
  }
};

#define PARAM_IMPL(...)                                                                              \
  SELF_MACRO_DEFINE_SELF(Self, public)                                                               \
  static const ::shards::TIterableParam<Core> *getIterableParams(size_t &outNumParams) {             \
    static ::shards::TIterableParam<Core> result[] = {__VA_ARGS__};                                  \
    outNumParams = std::extent<decltype(result)>::value;                                              \
    return result;                                                                                   \
  }                                                                                                  \
  PARAM_PARAMS()                                                                                     \
  PARAM_GET_SET()

#define PARAM_IMPL_DERIVED(BaseClass, ...)                                                           \
  SELF_MACRO_DEFINE_SELF(Self, public)                                                               \
  static const ::shards::TIterableParam<Core> *getIterableParams(size_t &outNumParams) {             \
    static std::vector<::shards::TIterableParam<Core>> combined = []() {                             \
      static ::shards::TIterableParam<Core> addParams[] = {__VA_ARGS__};                             \
      size_t numAddParams = std::extent<decltype(addParams)>::value;                                  \
                                                                                                     \
      size_t numBaseParams{};                                                                        \
      auto *baseParams = BaseClass::getIterableParams(numBaseParams);                                \
      std::vector<::shards::TIterableParam<Core>> result;                                            \
      result.resize(numBaseParams + numAddParams);                                                   \
      for (size_t i = 0; i < numBaseParams; ++i)                                                     \
        result[i] = baseParams[i];                                                                   \
      for (size_t i = 0; i < numAddParams; ++i)                                                      \
        result[numBaseParams + i] = addParams[i];                                                    \
      return result;                                                                                 \
    }();                                                                                             \
    outNumParams = combined.size();                                                                  \
    return combined.data();                                                                          \
  }                                                                                                  \
  PARAM_PARAMS()                                                                                     \
  PARAM_GET_SET()

#define PARAM_IMPL_DERIVED_PREPEND(BaseClass, ...)                                                   \
  SELF_MACRO_DEFINE_SELF(Self, public)                                                               \
  static const ::shards::TIterableParam<Core> *getIterableParams(size_t &outNumParams) {             \
    static std::vector<::shards::TIterableParam<Core>> combined = []() {                             \
      static ::shards::TIterableParam<Core> prependParams[] = {__VA_ARGS__};                         \
      size_t numPrependParams = std::extent<decltype(prependParams)>::value;                          \
                                                                                                     \
      size_t numBaseParams{};                                                                        \
      auto *baseParams = BaseClass::getIterableParams(numBaseParams);                                \
      std::vector<::shards::TIterableParam<Core>> result;                                            \
      result.resize(numBaseParams + numPrependParams);                                               \
      for (size_t i = 0; i < numPrependParams; ++i)                                                  \
        result[i] = prependParams[i];                                                                \
      for (size_t i = 0; i < numBaseParams; ++i)                                                     \
        result[numPrependParams + i] = baseParams[i];                                                \
      return result;                                                                                 \
    }();                                                                                             \
    outNumParams = combined.size();                                                                  \
    return combined.data();                                                                          \
  }                                                                                                  \
  PARAM_PARAMS()                                                                                     \
  PARAM_GET_SET()

#define PARAM_IMPL_FOR(_name)                                                                                                \
  ::shards::TIterableParam<Core>::create<decltype(_name)>([](void *obj) -> void * { return (void *)&((Self *)obj)->_name; }, \
                                                          &_name##ParameterInfo)

#define PARAM_PARAMS()                                                                          \
  static SHParametersInfo parameters() {                                                        \
    static SHParametersInfo result = []() {                                                     \
      SHParametersInfo result{};                                                                \
      size_t numParams;                                                                         \
      const auto *params = getIterableParams(numParams);                                        \
      Core::paramsResize(&result, numParams);                                                   \
      for (size_t i = 0; i < numParams; i++) {                                                  \
        result.elements[i] = *const_cast<::shards::ParameterInfo *>(params[i].paramInfo);       \
      }                                                                                         \
      return result;                                                                            \
    }();                                                                                        \
    return result;                                                                              \
  }

#define PARAM_REQUIRED_VARIABLES()                          \
  ::shards::TExposedInfo<Core> _requiredVariables;          \
  SHExposedTypesInfo requiredVariables() { return (SHExposedTypesInfo)_requiredVariables; }

#define PARAM_COMPOSE_REQUIRED_VARIABLES(__data)                                                                   \
  {                                                                                                                \
    size_t numParams;                                                                                              \
    const auto *params = getIterableParams(numParams);                                                             \
    _requiredVariables.clear();                                                                                    \
    for (size_t i = 0; i < numParams; i++) {                                                                       \
      if (params[i].collectRequirements) {                                                                         \
        params[i].collectRequirements(params[i], __data, _requiredVariables, params[i].resolveParamInShard(this)); \
      }                                                                                                            \
    }                                                                                                              \
  }

#define PARAM_COMPOSE_MERGE_REQUIRED(__shardsVar)               \
  {                                                             \
    auto __required = __shardsVar.composeResult().requiredInfo; \
    for (auto &req : __required) {                              \
      _requiredVariables.push_back(req);                        \
    }                                                           \
  }

#define PARAM_GET_SET()                                                                          \
  void setParam(int index, const SHVar &value) {                                                 \
    size_t numParams;                                                                            \
    const auto *params = getIterableParams(numParams);                                           \
    if (index >= 0 && index < int(numParams)) {                                                  \
      params[index].setParam(params[index].resolveParamInShard(this), value);                    \
    } else {                                                                                     \
      throw ::shards::InvalidParameterIndex();                                                   \
    }                                                                                            \
  }                                                                                              \
  SHVar getParam(int index) {                                                                    \
    size_t numParams;                                                                            \
    const auto *params = getIterableParams(numParams);                                           \
    if (index >= 0 && index < int(numParams)) {                                                  \
      return params[index].getParam(params[index].resolveParamInShard(this));                    \
    } else {                                                                                     \
      throw ::shards::InvalidParameterIndex();                                                   \
    }                                                                                            \
  }

#define PARAM_WARMUP(_ctx)                                                    \
  {                                                                           \
    size_t numParams;                                                         \
    const auto *params = getIterableParams(numParams);                        \
    for (size_t i = 0; i < numParams; i++)                                    \
      params[i].warmup(params[i].resolveParamInShard(this), _ctx);            \
  }

#define PARAM_CLEANUP(_ctx)                                                   \
  {                                                                           \
    size_t numParams;                                                         \
    const auto *params = getIterableParams(numParams);                        \
    for (size_t i = 0; i < numParams; i++) {                                  \
      size_t iRev = (numParams - 1) - i;                                      \
      params[iRev].cleanup(params[iRev].resolveParamInShard(this), _ctx);     \
    }                                                                         \
  }

#endif /* B0328A63_0B69_4191_94D5_38783B9F20C9 - skip macros if internal params.hpp included */

} // namespace shards

#endif /* SH_DLL_PARAMS_HPP */
