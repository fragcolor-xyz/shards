#include "trait.hpp"
#include "runtime.hpp"
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
  if (trait_.name)
    std::free((void *)trait_.name);
  for (auto &item : trait_.variables) {
    freeTraitVariable(item);
  }
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
} // namespace shards