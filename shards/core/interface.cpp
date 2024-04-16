#include "interface.hpp"
#include "runtime.hpp"
#include <shards/iterator.hpp>
#include <string.h>
#include <cstdlib>

namespace shards {
void freeInterfaceVariable(SHInterfaceVariable &ivar) {
  std::free((void *)ivar.name);
  freeDerivedInfo(ivar.type);
}
SHInterfaceVariable cloneInterfaceVariable(const SHInterfaceVariable &other) {
  SHInterfaceVariable clone{
      .name = strdup(other.name),
      .type = cloneTypeInfo(other.type),
  };
  return clone;
}
void freeInterface(SHInterface &interface_) {
  if (interface_.name)
    std::free((void *)interface_.name);
  for (auto &item : interface_.variables) {
    freeInterfaceVariable(item);
  }
}
SHInterface cloneInterface(const SHInterface &other) {
  SHInterface result{
      .name = strdup(other.name),
  };

  result.id[0] = other.id[0];
  result.id[1] = other.id[1];
  for (auto &var : other.variables) {
    arrayPush(result.variables, cloneInterfaceVariable(var));
  }
  return result;
}
} // namespace shards