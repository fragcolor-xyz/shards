#include "foundation.hpp"
#include "variable_accessor.hpp"
#include <string>
#include <optional>

namespace shards::compose {

SHTypeInfo *resolveVariableSubPath(const SHTypeInfo &type, const VA_SubPath &subPath) {
    // Handle table types
    if (type.basicType == SHType::Table) {
        // Check if we have keys and types for this table
        if (type.table.keys.len > 0 && type.table.keys.len == type.table.types.len) {
            // First, look for exact match
            std::optional<SHTypeInfo*> magicNoneType;
            
            for (uint32_t i = 0; i < type.table.keys.len; i++) {
                auto &key = type.table.keys.elements[i];
                
                // Check for direct key match
                if (subPath.key == key) {
                    return &type.table.types.elements[i];
                } 
                // Check for "magic none" pattern (wildcard key)
                else if (key.valueType == SHType::None) {
                    if (!magicNoneType) {
                        magicNoneType = &type.table.types.elements[i];
                    } else {
                        // If we have multiple None keys with different types,
                        // we can't determine which one to use - this would need CoreInfo::AnyType
                        if (*(*magicNoneType) != type.table.types.elements[i]) {
                            magicNoneType.reset();
                            break;
                        }
                    }
                }
            }
            
            // If we found a magic none key and didn't find an exact match
            if (magicNoneType) {
                return *magicNoneType;
            }
        } 
        // If table has types but no keys, try to use the single type if there is only one
        else if (type.table.types.len == 1) {
            return &type.table.types.elements[0];
        }
        
        // No matching key or usable type found
        return nullptr;
    }
    // Handle sequence types 
    else if (type.basicType == SHType::Seq && subPath.key.valueType == SHType::Int) {
        if (type.seqTypes.len == 1) {
            return &type.seqTypes.elements[0];
        }
    }
    
    // For any other type or no match, return nullptr
    return nullptr;
}

} // namespace shards::compose
