#ifndef A16CC8A4_FBC4_4500_BE1D_F565963C9C16
#define A16CC8A4_FBC4_4500_BE1D_F565963C9C16

#include "foundation.hpp"
#include "runtime.hpp"
#include <array>

namespace shards {

namespace detail {
template <bool Required1> struct RequiredFlags {};
template <> struct RequiredFlags<false> {
  bool found;
};
} // namespace detail

// Defines a reference to a required variable
// Warmup references the variable, cleanup releases the reference
// use  getRequiredVariable to get the type
template <typename T, const SHTypeInfo &VariableType, const char *VariableName, bool Required = true>
struct RequiredContextVariable {
private:
  using Flags = detail::RequiredFlags<Required>;
  SHVar *variable{};
  Flags requiredFlags;

public:
  RequiredContextVariable() {}

  void warmup(SHContext *context, ParamVar *paramOverride = nullptr) {
    if (paramOverride && paramOverride->isVariable()) {
      variable = shards::referenceVariable(context, paramOverride->variableName());
    } else {
      // If optional and not found during compose, early out
      if constexpr (!Required) {
        if (!requiredFlags.found)
          return;
      }
      variable = shards::referenceVariable(context, VariableName);
    }
    warmupCommon();
  }

  void compose(const SHInstanceData &data, ExposedInfo &outRequired, ParamVar *paramOverride = nullptr) {
    std::optional<SHExposedTypeInfo> exposed;
    if (paramOverride && paramOverride->isVariable()) {
      exposed = findExposedVariable(data.shared, *paramOverride);
    }

    if (!exposed) {
      exposed = findExposedVariable(data.shared, VariableName);
      if constexpr (!Required) {
        requiredFlags.found = exposed.has_value();
      } else {
        if (!exposed) {
          throw ComposeError(fmt::format("Required variable {} not found", VariableName));
        }
      }
    }

    if (exposed && exposed.value().exposedType != VariableType) {
      throw ComposeError(fmt::format("Required variable {} has the wrong type. Expected: {}, was {}", VariableName, VariableType,
                                     exposed.value().exposedType));
    }

    if (exposed) {
      outRequired.push_back(exposed.value());
    }
  }

  void cleanup(SHContext *context = nullptr) {
    if (variable) {
      shards::releaseVariable(variable);
      variable = nullptr;
    }
  }

  T *get() const {
    shassert(variable);
    return &varAsObjectChecked<T>(*variable, VariableType);
  }

  const SHVar &asVar() {
    shassert(variable);
    return *variable;
  }

  operator bool() const { return variable; }
  T *operator->() const { return get(); }
  operator T &() const { return *get(); }

  static constexpr const char *variableName() { return VariableName; }

  static constexpr SHExposedTypeInfo getExposedTypeInfo() {
    SHExposedTypeInfo typeInfo{
        .name = VariableName,
        .exposedType = VariableType,
        .isProtected = true,
        .global = true,
    };
    return typeInfo;
  }

private:
  void warmupCommon() {
    if constexpr (!Required) {
      if (variable->valueType == SHType::None) {
        cleanup(nullptr);
      }
    }
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

inline void getObjectTypes(std::vector<SHTypeInfo> &out, const SHTypeInfo &type) {
  switch (type.basicType) {
  case SHType::Object:
    out.push_back(type);
    break;
  case SHType::Seq:
    for (auto &t : type.seqTypes)
      getObjectTypes(out, t);
    break;
  case SHType::Table:
    for (auto &t : type.table.types)
      getObjectTypes(out, t);
    break;
  default:
    break;
  }
}

inline bool hasContextVariables(const SHTypeInfo &type) {
  switch (type.basicType) {
  case SHType::ContextVar:
    return true;
  case SHType::Seq:
    for (auto &t : type.seqTypes)
      if (hasContextVariables(t))
        return true;
    break;
  case SHType::Table:
    for (auto &t : type.table.types)
      if (hasContextVariables(t))
        return true;
    break;
  default:
    break;
  }
  return false;
}

} // namespace shards

#endif /* A16CC8A4_FBC4_4500_BE1D_F565963C9C16 */
