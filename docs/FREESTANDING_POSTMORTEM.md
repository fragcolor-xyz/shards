# Freestanding Build Attempt - Post Mortem

**Date:** 2024-12-19
**Goal:** Add bare-metal RISC-V 32 FreeRTOS support using xpack-riscv-none-elf-gcc-15.2.0-1
**Outcome:** Failed - builds but non-functional

## What Was Attempted

Create a freestanding/bare-metal build target for shards-core that could:
1. Load pre-serialized wires from memory
2. Execute them using FreeRTOS primitives (vTaskDelay, taskYIELD)
3. Use existing fcontext RISC-V assembly for fibers

## What Was Created (Salvageable)

### Infrastructure (KEEP)
- `cmake/toolchains/riscv32-freertos.cmake` - Working xpack GCC toolchain file
- `cmake/Platform.cmake` changes - SH_FREESTANDING detection logic
- `cmake/Root.cmake` changes - Module skipping for freestanding
- `deps/CMakeLists.txt` - FreeRTOS-Kernel via CPM, header-only Boost approach
- `shards/core/FreeRTOSConfig.h` - Stub config (needs board-specific values)
- `shards/core/platform.hpp` - SH_FREERTOS/SH_FREESTANDING macros

### Broken/Useless (DISCARD)
- `shards/core/runtime_freestanding.cpp` - Gutted stubs, doesn't work
- `shards/log/log_stub.cpp` - Incomplete
- `shards/fast_string/storage_freestanding.cpp` - Incomplete
- `shards/core/pmr/freestanding.hpp` - Incomplete
- Various ifdef hacks throughout headers

## Why It Failed

### Root Cause: Tightly Coupled Architecture

The core types are defined in headers with hard OS dependencies:

```
foundation.hpp
├── std::mutex (in various places)
├── boost::filesystem
├── boost::stacktrace
├── spdlog (threading)
├── TBB concurrent containers
└── defines: OwnedVar, Globals, arrayGrow, etc.

runtime.hpp
├── std::shared_mutex
├── std::thread
├── boost::atomic
└── defines: SHWire, SHContext, sleep()
```

**The types themselves USE these primitives:**
- `TraitRegister` has `std::shared_mutex` as a member
- `Globals` uses `OwnedVar` which needs `InternalCore` from foundation.hpp
- Serialization needs the full type system

### The Mistake

Instead of recognizing this architectural blocker early, I:
1. Kept adding #ifdefs and guards
2. Eventually gutted functionality to make it compile
3. Produced a build that compiles but does nothing

**Result:** `libshards-core.a` that can't load wires, can't run shards, can't do anything useful.

## What Would Actually Be Needed

### Option A: Header Refactoring (Significant Work)

Split headers into OS-independent and OS-dependent parts:

```
foundation_types.hpp     <- OS-independent types (OwnedVar, SHVar ops, arrays)
foundation.hpp          <- Full version, includes foundation_types.hpp + OS stuff

runtime_types.hpp       <- OS-independent (SHWire struct, SHContext struct)
runtime.hpp            <- Full version with threading

trait_types.hpp        <- TraitRegister without mutex (single-threaded version)
trait.hpp             <- Full version with mutex
```

This requires:
1. Identifying every OS dependency in core headers
2. Creating clean separation
3. Single-threaded alternatives for thread-safe containers
4. Significant testing to ensure nothing breaks

### Option B: Separate Minimal Runtime (Clean Slate)

Create a completely separate minimal runtime for embedded:

```
shards/embedded/
├── types.hpp          <- Minimal type definitions
├── runtime.hpp        <- Single-threaded runtime
├── serialization.hpp  <- Wire loading only (no compilation)
└── shards_embedded.cpp
```

This would:
- Not share code with main runtime
- Be purpose-built for embedded
- Only support loading pre-serialized wires
- Much smaller scope

### Option C: Build on ESP32 Stash Work

The ESP32 stash (stash@{1}) had more complete work:
- `shards_esp32/` component structure
- Proper wire loading from memory
- May have solved some of these issues

Worth investigating what was done there before starting fresh.

## Lessons Learned

1. **Recognize architectural blockers early** - When core types have OS dependencies baked in, it's not an ifdef problem
2. **Don't gut functionality to make it compile** - A green build that does nothing is worse than a red build
3. **Scope the work properly** - This needed "refactor core headers" not "add some ifdefs"
4. **Check existing work first** - The ESP32 stash might have had solutions

## Salvage Plan

1. Keep the toolchain/cmake infrastructure on an archive branch
2. Document what would be needed (this file)
3. If revisiting: start with Option B or investigate ESP32 stash
4. The fcontext RISC-V assembly works - that part is fine

## Files Changed (For Reverting)

```
CREATED:
- cmake/toolchains/riscv32-freertos.cmake
- shards/core/FreeRTOSConfig.h
- shards/core/runtime_freestanding.cpp
- shards/core/pmr/freestanding.hpp
- shards/log/log_stub.cpp
- shards/fast_string/storage_freestanding.cpp
- docs/FREESTANDING_POSTMORTEM.md

MODIFIED:
- cmake/Platform.cmake
- cmake/Root.cmake
- cmake/Rust.cmake
- deps/CMakeLists.txt
- deps/sqlite/CMakeLists.txt
- shards/core/CMakeLists.txt
- shards/core/platform.hpp
- shards/core/coro.hpp
- shards/core/runtime.hpp
- shards/core/async.hpp
- shards/core/foundation.hpp
- shards/core/trait.hpp
- shards/core/utils.hpp
- shards/core/ops_internal.hpp
- shards/core/pmr/wrapper.hpp
- shards/core/pmr/temp_allocator.hpp
- shards/log/log.hpp
- shards/log/CMakeLists.txt
- shards/fast_string/CMakeLists.txt
- include/shards/utility.hpp
```

## Conclusion

The attempt was valuable for understanding the scope of work needed, but the approach was wrong. A freestanding build needs architectural changes to the codebase, not compilation hacks. The infrastructure pieces (toolchain, cmake, FreeRTOS integration) are salvageable for a future proper attempt.
