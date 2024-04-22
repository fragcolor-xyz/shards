#include "trait.hpp"
#include "runtime.hpp"
#include "exposed_type_utils.hpp"
#include "ops_internal.hpp"
#include <shards/iterator.hpp>
#include <string.h>
#include <cstdlib>

namespace shards {
void freeTraitVariable(SHTraitVariable &ivar) {
  std::free((void *)ivar.name);
  freeDerivedInfo(ivar.type);
}
SHTraitVariable cloneTraitVariable(const SHTraitVariable &other) {
  SHTraitVariable clone{
      .name = strdup(other.name),
      .type = cloneTypeInfo(other.type),
  };
  return clone;
}
void freeTrait(SHTrait &trait_) {
  if (trait_.name) {
    std::free((void *)trait_.name);
    trait_.name = nullptr;
  }
  for (auto &item : trait_.variables) {
    freeTraitVariable(item);
  }
  arrayFree(trait_.variables);
}
SHTrait cloneTrait(const SHTrait &other) {
  SHTrait result{
      .name = strdup(other.name),
  };

  result.id[0] = other.id[0];
  result.id[1] = other.id[1];
  for (auto &var : other.variables) {
    arrayPush(result.variables, cloneTraitVariable(var));
  }
  return result;
}

template <typename S, typename... Args> inline auto formatLineInto(std::string &str, const S &format_str, Args &&...args) {
  if (!str.empty())
    str.push_back('\n');
fmt::format_to(std::back_inserter(str), format_str, std::forward<Args>(args)...);
}

bool TraitMatcher::operator()(SHExposedTypesInfo exposedVariables, const SHTrait &trait) {
  TypeMatcher tm;
  tm.ignoreFixedSeq = true;

  error.clear();
  for (auto &v : trait.variables) {
    auto exposed = findExposedVariable(exposedVariables, v.name);
    if (!exposed) {
      formatLineInto(error, "Trait variable '{}' not found", v.name);
      continue;
    }
    if(!exposed->isMutable) {
      formatLineInto(error, "Trait variable '{}' is not mutable", v.name);
      continue;
    }
    if (!tm.match(v.type, exposed->exposedType)) {
      formatLineInto(error, "Trait variable '{}' type mismatch, {} is not compatible with trait type {}", v.name,
                     exposed->exposedType, v.type);
      continue;
    }
  }
  return error.empty();
}

} // namespace shards