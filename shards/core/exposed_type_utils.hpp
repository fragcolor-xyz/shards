#ifndef A16CC8A4_FBC4_4500_BE1D_F565963C9C16
#define A16CC8A4_FBC4_4500_BE1D_F565963C9C16

#include "foundation.hpp"
#include "runtime.hpp"
#include <array>

namespace shards {

// Defines a reference to a required variable
// Warmup references the variable, cleanup releases the refence
// use  getRequiredVariable to get the type
template <typename T, const SHTypeInfo &VariableType, const char *VariableName, bool Required = true>
struct RequiredContextVariable {
private:
  SHVar *variable{};

public:
  RequiredContextVariable() {}

  void warmup(SHContext *context) {
    assert(!variable);
    variable = shards::referenceVariable(context, VariableName);

    if constexpr (Required) {
      // Ensure correct runtime type
      (void)varAsObjectChecked<T>(*variable, VariableType);
    } else {
      if (variable->valueType == SHType::None) {
        cleanup();
      }
    }
  }

  void cleanup() {
    if (variable) {
      shards::releaseVariable(variable);
      variable = nullptr;
    }
  }

  const SHVar &asVar() {
    assert(variable);
    return *variable;
  }

  operator bool() const { return variable; }
  T *operator->() const {
    assert(variable); // Missing call to warmup?
    return reinterpret_cast<T *>(variable->payload.objectValue);
  }
  operator T &() const {
    assert(variable); // Missing call to warmup?
    return *reinterpret_cast<T *>(variable->payload.objectValue);
  }

  static constexpr SHExposedTypeInfo getExposedTypeInfo() {
    SHExposedTypeInfo typeInfo{
        .name = VariableName,
        .exposedType = VariableType,
        .global = true,
    };
    return typeInfo;
  }
};

namespace detail {
template <size_t N> struct StaticExposedArray : public std::array<SHExposedTypeInfo, N> {
  operator SHExposedTypesInfo() const {
    return SHExposedTypesInfo{.elements = const_cast<SHExposedTypeInfo *>(this->data()), .len = N, .cap = 0};
  }
};
} // namespace detail

// Returns a storage type that is convertible to SHExposedTypesInfo
// Example:
//   SHExposedTypesInfo requiredVariables() {
//     static auto e = exposedTypesOf(SomeExposedTypeInfo, SHExposedTypeInfo{...});
//     return e;
//   }
//
//   RequiredContextVariable<GraphicsContext, GraphicsContext::Type, Base::graphicsContextVarName> graphicsContext;
//   SHExposedTypesInfo requiredVariables() {
//     static auto e = exposedTypesOf(decltype(graphicsContext)::getExposedTypeInfo());
//     return e;
//   }
template <typename... TArgs> constexpr auto exposedTypesOf(TArgs... args) {
  detail::StaticExposedArray<sizeof...(TArgs)> result;
  size_t index{};
  (
      [&](SHExposedTypeInfo typeInfo) {
        result[index] = typeInfo;
        ++index;
      }(SHExposedTypeInfo(args)),
      ...);
  return result;
}

inline void mergeIntoExposedInfo(ExposedInfo &outInfo, const ParamVar &paramVar, const SHTypeInfo &type, bool isMutable = false) {
  if (paramVar.isVariable())
    outInfo.push_back(ExposedInfo::Variable(paramVar.variableName(), SHCCSTR("The required variable"), type, isMutable));
}

inline void mergeIntoExposedInfo(ExposedInfo &outInfo, const ShardsVar &shardsVar) {
  for (auto &info : shardsVar.composeResult().requiredInfo)
    outInfo.push_back(info);
}

inline void mergeIntoExposedInfo(ExposedInfo &outInfo, const SHExposedTypesInfo &otherTypes) {
  for (size_t i = 0; i < otherTypes.len; i++)
    outInfo.push_back(otherTypes.elements[i]);
}

} // namespace shards

#endif /* A16CC8A4_FBC4_4500_BE1D_F565963C9C16 */
