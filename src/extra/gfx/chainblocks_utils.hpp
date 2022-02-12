#pragma once
#include <chainblocks.hpp>
#include <foundation.hpp>

namespace gfx {
// Retrieves a value directly or from a context variable from a table by name
// returns false if the table does not contain an entry for that key
inline bool getFromTable(CBContext *cbContext, const CBTable &table, const char *key, CBVar &outVar) {
	if (table.api->tableContains(table, key)) {
		const CBVar *var = table.api->tableAt(table, key);
		if (var->valueType == CBType::ContextVar) {
			CBVar *refencedVariable = chainblocks::referenceVariable(cbContext, var->payload.stringValue);
			outVar = *var;
			chainblocks::releaseVariable(refencedVariable);
		}
		outVar = *var;
		return true;
	}
	return false;
}
} // namespace gfx
