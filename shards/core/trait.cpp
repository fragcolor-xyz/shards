#include "trait.hpp"
#include "runtime.hpp"
#include "exposed_type_utils.hpp"
#include "ops_internal.hpp"
#include <shards/iterator.hpp>
#include <string.h>
#include <cstdlib>

namespace shards {
void freeTraitVariable(SHTraitVariable &ivar) {
  if (ivar.name.string) {
    delete[] ivar.name.string;
    ivar.name.string = nullptr;
    ivar.name.len = 0;
  }
  freeDerivedInfo(ivar.type);
}
SHTraitVariable cloneTraitVariable(const SHTraitVariable &other) {
  SHTraitVariable clone{
      .name = swlDuplicate(other.name),
      .type = cloneTypeInfo(other.type),
  };
  return clone;
}
void freeTrait(SHTrait &trait_) {
  swlFree(trait_.name);
  for (auto &item : trait_.variables) {
    freeTraitVariable(item);
  }
  arrayFree(trait_.variables);
}
SHTrait cloneTrait(const SHTrait &other) {
  SHTrait result{
      .name = swlDuplicate(other.name),
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
    auto exposed = findExposedVariable(exposedVariables, toStringView(v.name));
    if (!exposed) {
      formatLineInto(error, "Trait variable '{}' not found", v.name);
      continue;
    }
    if (!exposed->isMutable) {
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

TraitRegister &TraitRegister::instance() {
  static TraitRegister tc;
  return tc;
}

} // namespace shards