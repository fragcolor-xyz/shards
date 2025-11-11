# Shard Metadata Migration Summary

## Overview
Migrated static/compile-time fields from per-instance `Shard` struct to shared `ShardMetadata` struct to reduce memory usage and improve cache locality.

## Changes Made

### 1. ShardMetadata struct (`include/shards/shards.h:787-807`)
**Expanded to include:**
- `staticName` - Static name for shards that don't override name()
- `nameLength` - Cached name length (uint32_t)
- `hash` - Type-level hash (crc32)
- `help` - Static help documentation
- `inputHelp` - Static input documentation
- `outputHelp` - Static output documentation
- `parameters` - Parameter structure (static for a shard type)

### 2. ShardWrapper (`include/shards/shardwrapper.hpp`)

**Added:**
- `initMetadata()` static function to populate metadata fields during registration (lines 50-69)

**Modified:**
- `create()` function now uses `metadata->*` fields for shards without instance overrides:
  - `name` → `metadata->staticName` (line 82)
  - `hash` → `metadata->hash` (line 90)
  - `help` → `metadata->help` (line 97)
  - `inputHelp` → `metadata->inputHelp` (line 105)
  - `outputHelp` → `metadata->outputHelp` (line 113)
  - `parameters` → `metadata->parameters` (line 182)
- Also sets `result->nameLength` directly from metadata (line 83)

**Registration macros updated:**
- `REGISTER_SHARD` now calls `initMetadata()` (line 373)
- `REGISTER_SHARD_ALIAS` updated similarly (lines 378-382)

### 3. Legacy Macros (`shards/core/shards_macros.hpp`)

**All runtime shard macros updated:**
- `RUNTIME_SHARD` - Added metadata initialization (lines 15-25)
- `RUNTIME_CORE_SHARD` - Updated metadata initialization (lines 48-58)
- `RUNTIME_SHARD_TYPE` - Updated metadata initialization (lines 92-102)
- `RUNTIME_CORE_SHARD_TYPE` - Updated metadata initialization (lines 137-147)

**All factory macros updated to use metadata:**
- Name, hash, help, inputHelp, outputHelp, parameters now reference `shard->metadata->*`
- `nameLength` set directly from metadata

**Global replacement:**
- All occurrences of parameter/help lambdas updated to use metadata (4 occurrences)

## Benefits

1. **Memory Savings** - One metadata copy per shard type instead of per instance
2. **Cache Locality** - Hot instance data stays together, cold metadata separate
3. **Clear Semantics** - Explicit separation of type-level vs instance-level data
4. **Optimization Potential** - Static data can live in read-only memory

## What Stays in Shard Struct

- `inputTypes`/`outputTypes` - Can change dynamically after compose()
- `properties` - Can be instance-variant
- All lifecycle function pointers (setup, destroy, warmup, activate, cleanup, etc.)
- Instance-specific fields (refCount, owned, line, column, file, debuggerId, etc.)

## Build Considerations

### Potential Issues:
1. **ABI Change** - This changes the `ShardMetadata` struct layout
2. **Initialization Order** - `initMetadata()` must be called during registration
3. **Null Pointer** - Code must ensure `metadata` pointer is set before dereferencing

### Testing Checklist:
- [ ] Verify all shards registered via `REGISTER_SHARD` work correctly
- [ ] Verify legacy macros (`RUNTIME_SHARD`, etc.) work correctly
- [ ] Test shards with custom name()/hash()/help() methods
- [ ] Test shards without custom methods (using static defaults)
- [ ] Verify metadata fields are properly initialized
- [ ] Check for null pointer dereferences on `shard->metadata`
- [ ] Run full test suite: `just tests`

### Debugging:
If you see crashes related to null `metadata` pointer:
1. Check that `REGISTER_SHARD` macro is calling `initMetadata()`
2. Verify `result->metadata = &metadata` is set in create() functions
3. Ensure metadata is initialized before first use

## Next Steps

When building locally:
1. Run `just format` to format code
2. Run `just build` to compile
3. Run `just tests` to verify functionality
4. Check for any compiler warnings about uninitialized metadata
5. Test with real shard usage to verify runtime behavior

## Notes

- The abstraction via `ShardWrapper` and macros means most shard implementations don't need changes
- Only the wrapper/macro infrastructure was updated
- Shards that override methods (name(), hash(), help(), etc.) still work via their override functions
- Shards without overrides now benefit from shared metadata storage
