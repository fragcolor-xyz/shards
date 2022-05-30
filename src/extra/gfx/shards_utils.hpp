#ifndef EXTRA_GFX_SHARDS_UTILS
#define EXTRA_GFX_SHARDS_UTILS

#include <shards.hpp>
#include <foundation.hpp>

namespace gfx {
// Retrieves a value directly or from a context variable from a table by name
// returns false if the table does not contain an entry for that key
inline bool getFromTable(SHContext *shContext, const SHTable &table, const char *key, SHVar &outVar) {
  if (table.api->tableContains(table, key)) {
    const SHVar *var = table.api->tableAt(table, key);
    if (var->valueType == SHType::ContextVar) {
      SHVar *refencedVariable = shards::referenceVariable(shContext, var->payload.stringValue);
      outVar = *var;
      shards::releaseVariable(refencedVariable);
    }
    outVar = *var;
    return true;
  }
  return false;
}
} // namespace gfx

#endif // EXTRA_GFX_SHARDS_UTILS
