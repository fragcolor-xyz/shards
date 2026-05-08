# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build and Development Commands

### Essential Commands
- **Build**: `just build` - Build shards, binary will be found at `build/Debug/shards`
- **Format**: `just format` - Format C++ code with clang-format
- **Tests**: `just tests` - Run Shards language test suite (note: tests take time)
- **Update**: `./update.sh` - Update git submodules and dependencies

### Just Commands
- `just pull` - Git pull and update dependencies in one command
- `just build` - Build shards, binary will be found at `build/Debug/shards`
- `just format` - Format C++ code with clang-format
- `just tests` - Run test suite (note: tests take time)
- `just cargo-check` - Check Rust code with correct toolchain (use this for Rust development)
- `just build-docker-image` - Build Docker image with current commit

## Architecture Overview

### Language and Runtime
Shards is a flow-based programming language with a unique data flow paradigm. Code is written in `.shs` files using pipe operators (`|`) to chain transformations:

```shards
// Simple data flow
["Hello" name "!"] | String.Join | Log

// Wire system for concurrency
@wire(main-loop {
    input | Transform | Process | Output
} Looped: true)
```

### Core Technologies
- **C++ Core**: Runtime, type system, and core language implementation in `shards/core/`
- **Rust Modules**: Functionality modules (CSV, network, physics, etc.) in `shards/modules/`
- **Swift Bindings**: iOS/macOS integration via Swift wrapper files
- **Graphics**: Custom GFX system built on WebGPU (wgpu-native fork)

### Module Architecture
- **Core Runtime**: `shards/core/` - Language runtime, wire system, type checking
- **Modules**: `shards/modules/` - Self-contained Rust crates for specific functionality
- **Graphics**: `shards/gfx/` - Complete graphics subsystem with Rust backend
- **Tests**: `shards/tests/*.shs` - Comprehensive test suite in Shards language

### Build System
- **CMake**: Primary build system with complex multi-platform support
- **Dependencies**: Git submodules in `deps/` (llama.cpp, whisper.cpp, tracy, etc.)
- **Target**: Produces `shards` executable and various libraries

## Development Workflow

### Initial Setup
1. `just build` - Creates initial debug build (tools are built automatically)
2. `just tests` - Verify installation works

### Regular Development
1. Make code changes
2. `just format` - Format C++ code before committing
3. `just build` - Rebuild after changes
4. `just tests` - Verify tests pass
5. Git commit/push

### Rust Development
When working on Rust modules or the graphics subsystem:
1. Make changes to Rust code in `shards/modules/` or `shards/gfx/`
2. `just cargo-check` - Check Rust code with the correct toolchain
3. `just build` - Full rebuild to integrate Rust changes
4. `just tests` - Run test suite to verify changes

**IMPORTANT**: Always use `just cargo-check` instead of `cargo check` directly. The project uses a specific Rust toolchain, and the just command ensures the correct toolchain is used.

**CRITICAL — Async in Rust shards**: NEVER use `block_on()`, `BlockingModel`, or any thread-blocking call inside a shard's `activate()`. Shards uses coroutine scheduling — blocking stalls the entire wire. Use `shards::core::run_future(context, async { ... }, on_cancel)` with a shared `TOKIO_RUNTIME`. See `shards/modules/http/src/lib.rs` for the canonical pattern.

### Testing
- **Test Files**: `shards/tests/*.shs` contain language-level tests
- **Test Runner**: `just tests` runs the test suite through shards executable
- **Test Types**: Core language, graphics (gfx-*), UI, physics, networking
- Tests create tag files in `shards/tests/tag_ok/` and `shards/tests/tag_err/`
- **Note**: Tests take time to complete

### Debugging and Logging
- **Log Levels**: Control logging verbosity with environment variables
- **Shards Log**: `LOG_shards=debug shards script.shs` - Enable debug logging for shards runtime
- **Log Levels**: `trace`, `debug`, `info`, `warning`, `error`
- Example: `LOG_shards=trace shards script.shs` - Show all trace-level logs including detailed path validation

## Key Directories

### Source Code
- `shards/core/` - C++ runtime and language implementation
- `shards/modules/` - Rust modules (csv, fs, network, physics, svg, etc.)
- `shards/gfx/` - Graphics subsystem and related Rust code
- `include/` - C++ headers and Swift interface files

### Build and Dependencies
- `deps/` - Git submodules for external libraries
- `build*/` - CMake build directories (multiple configurations)
- `cmake/` - CMake configuration and helper files

### Configuration
- `.clang-format` - C++ code formatting rules
- `rustfmt.toml` - Rust code formatting configuration
- `CMakeLists.txt` - Main CMake configuration

## Important Notes

### File Extensions
- `.shs` - Shards language source files
- `.cpp/.hpp` - C++ implementation files
- `.rs` - Rust module source files
- `.swift` - Swift binding files

### Git Submodules
This repository heavily uses git submodules. Always use `./update.sh` or `git submodule update --init --recursive` when switching branches or after pulling.

### Multi-Platform
The build system supports multiple platforms (macOS, Linux, Windows, iOS, etc.). Build artifacts may be in platform-specific directories.

### Swift Integration
Swift files provide iOS/macOS bindings. The main interface is in `include/shards/shards.swift` with module-specific implementations in various directories.

## Native macOS Build — Known Issues

### Asymmetric Debug-vs-Release Compile Failures (PCH staleness)

**Symptom:** `just build` fails in Debug while a previously-successful Release build still appears to work. The failures are typically C++ compile errors deep inside system headers like `<complex>`, `<vector>`, `<filesystem>` — code you didn't change.

**Root cause:** Several modules use precompiled headers via `target_precompile_headers(...)` (notably `shards/modules/gfx/CMakeLists.txt`). CMake/Ninja **do not track system headers under `/Library/Developer/CommandLineTools/...` or Xcode SDK paths as dependencies of the PCH.** When macOS / Command Line Tools / Xcode is updated, system headers change underneath the PCH but Ninja sees no tracked deps changed and reuses the stale PCH. Whichever build dir was *not* reconfigured since the OS update keeps "working" off the pre-update PCH; the freshly-rebuilt one fails.

**Diagnostic flow:**
```bash
# 1. Confirm one config is using a stale PCH:
stat -f "%Sm %N" build/{Debug,Release}/src/union/CMakeFiles/shards-cpp-union_shards-module-gfx.dir/cmake_pch.hxx.pch 2>/dev/null

# 2. Compare to actual CLT install time (file mtimes can be backdated by Apple's installer):
pkgutil --pkg-info=com.apple.pkg.CLTools_Executables | grep install-time
# Convert that Unix epoch to a date and compare to the PCH timestamp.

# 3. If PCH predates the CLT install, the "working" config is just stale.
# Wiping `build/<config>/` will reproduce the failure on that config.
```

**Fix:** wipe and reconfigure: `rm -rf build/<broken-config>/ && just build`. If the underlying header bug is real (see next section), this won't unstick you — you need a code-level workaround.

### CLT 26.x: libc++ `<complex>` Missing `__promote_t` Include

**Symptom:** Building `shards/modules/gfx/shader/{linalg,math,flow,core}_shards.cpp` fails with:
```
.../usr/include/c++/v1/complex:1105:38: error: use of undeclared identifier '__promote_t'
```
Pulled in via `Accelerate.framework → vecLib → Sparse/Solve.h → <complex>` (linalg.h transitively).

**Root cause:** Apple's libc++ shipped in **Command Line Tools 26.4.1** (and the matching Xcode SDK) has a bug where `<complex>` uses `__promote_t<>` directly but only includes `<__type_traits/conditional.h>`, not `<__type_traits/promote.h>` where `__promote_t` is defined. Both the CLT and Xcode SDK at this version are affected — switching SDKs does not help.

**Workaround applied:** `cmake/Platform.cmake` adds the following inside the `if(APPLE) ... endif()` block:
```cmake
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -include __type_traits/promote.h")
```
This forces `__promote_t`'s definition to be parsed before any `<complex>` usage. The flag is harmless once Apple fixes the SDK (the include just becomes a no-op). Remove the line at that point if you want to be tidy.

**Why `CMAKE_CXX_FLAGS` and not `add_compile_options(...)`?** The natural form
```cmake
add_compile_options($<$<COMPILE_LANGUAGE:CXX>:SHELL:-include __type_traits/promote.h>)
```
**does not work** in this codebase. CMake mangles options containing a space when they're wrapped in a *nested* generator expression — even with the `SHELL:` prefix. The output ends up as two broken args (`$<1:SHELL:-include` and `__type_traits/promote.h>`) because the inner expression's closing `>` is consumed by the outer `$<COMPILE_LANGUAGE:CXX>:...>`. `CMAKE_CXX_FLAGS` is C++-scoped by definition, so we get the "language filter" without needing a generator expression at all. **Generic CMake lesson: prefer `CMAKE_<LANG>_FLAGS` for language-scoped flags with embedded spaces; reserve `SHELL:` generator-expression form for non-nested uses.**

### CLT 26.x: AddressSanitizer Deadlocks at Startup

**Symptom:** Debug builds (which enable `-fsanitize=address` by default) hang at startup before any user code runs. Even `build/Debug/shards --help` hangs forever, with `LOG_shards=trace` producing zero output. The hang is in `dyld`'s `runAllInitializersForMain` and never returns.

**Root cause:** `sample <pid>` shows the deadlock chain:
```
__asan::AsanInitFromRtl → InitializeShadowMemory → get_dyld_hdr →
  dyld_shared_cache_iterate_text → _Block_copy → malloc →
    __sanitizer_mz_malloc → __asan::AsanInitFromRtl (recursive!) →
      __sanitizer::StaticSpinMutex::LockSlow → swtch_pri (spins forever)
```
The ASan runtime in `libclang_rt.asan_osx_dynamic.dylib` shipped with CLT 17 / macOS 26.4.1 calls `dyld_shared_cache_iterate_text` during shadow memory init. On the new dyld, that function uses `_Block_copy` which mallocs, which is intercepted by ASan and re-enters `AsanInitFromRtl` while the outer call still holds the init spinlock. Deadlock.

**Workaround:** use **Release** builds for testing until Apple ships a fixed `libclang_rt.asan` (`just build-rel` then `build/Release/shards <script>`). Release does not link ASan. Or, build Debug with ASan disabled — set `SH_USE_ASAN=OFF` if shards' CMake exposes that, or remove the `-fsanitize=address` flag in `cmake/Sanitizers.cmake` for macOS 26.x.

**Confirming you have the bug:** run `sample $(pgrep -n shards) 2 -mayDie` on the hung process — if you see `__sanitizer::StaticSpinMutex::LockSlow` and `swtch_pri` dominating the call counts, that's it.

**Confirming you have the bug:**
```bash
grep -c "promote.h" $(xcrun --show-sdk-path)/usr/include/c++/v1/complex
# 0 means buggy; >0 means fixed.
```

## Emscripten / WebAssembly Build

### Overview
Shards supports WebAssembly builds via Emscripten. The wasm build uses **emdawnwebgpu** (Dawn's WebGPU bindings for Emscripten) instead of Emscripten's built-in WebGPU support, because emdawnwebgpu provides the modern WebGPU API (with `WGPUStringView`, etc.) that matches wgpu-native v27+.

### Requirements
- **emsdk 4.0.10+**: Required for the `--use-port=emdawnwebgpu` flag
- emsdk should be cloned at `../emsdk` (or set `EMSDK_PATH` environment variable)

### Just Commands
- `just configure-wasm` - Configure cmake for wasm build (installs emsdk toolchain if needed)
- `just build-wasm` - Full wasm build (configures first)
- `just build-wasm-quick` - Quick rebuild (skips configure)
- `just setup-wasm-tests` - Install npm dependencies and puppeteer for browser tests
- `just test-wasm` - Run all wasm tests in headless browser
- `just test-wasm gfx-cube.shs` - Run specific test(s)

### Key Technical Details

**emdawnwebgpu vs Emscripten WebGPU:**
- Emscripten's built-in `webgpu.h` uses the old WebGPU API (null-terminated strings)
- emdawnwebgpu uses the modern API (`WGPUStringView`, `WGPUBufferMapCallbackInfo`, etc.)
- The `--use-port=emdawnwebgpu` flag is added in `shards/gfx/CMakeLists.txt`

**WebGPU Surface API:**
- Uses modern surface API: `wgpuSurfaceConfigure`, `wgpuSurfaceGetCurrentTexture`, `wgpuSurfacePresent`
- No deprecated `WGPUSwapChain` - surface configuration is unified across platforms
- Emscripten surface source: `WGPUEmscriptenSurfaceSourceCanvasHTMLSelector`

**Buffer Mapping:**
- Uses `wgpuBufferGetConstMappedRange` for both wgpu-native and emdawnwebgpu
- On emdawnwebgpu: allocates WASM memory and copies buffer data there
- Memory is automatically freed when buffer is unmapped

**Canvas Setup (gfx_events.js):**
- Must set `canvas.width`/`canvas.height` (intrinsic size) for WebGPU surface
- CSS size (via `getBoundingClientRect`) is only for layout
- Initial size must be set synchronously before graphics initialization

### Test Infrastructure
- Test server: `shards/tests/server/` - HTTP server for serving wasm and test files
- Test harness: `shards/tests/web/` - Puppeteer-based browser test runner
- Tests use wasmfs with fetch backend for filesystem access

### Debugging Wasm Builds
- Add `"SHELL:-s ASSERTIONS=2"` to link options for better error messages
- Check browser console for WebGPU errors
- The `pollLogs` function in `index.html` polls shards log messages from wasm

### Known Issues & Workarounds

**wasmfs `FS.Absolute` returns malformed paths for mounted directories:**
- Root cause: Mounted directories created via `wasmfs_create_directory` store names only in the dcache, not backend entries. The `getName()` function in wasmfs doesn't fall through to check dcache, returning empty strings.
- Symptom: `#("." | FS.Absolute)` returns `//shards/tests/.` instead of `/tfs/shards/tests/.`
- Upstream fix: https://github.com/nicolo-ribaudo/nicolo-nicolo-nicolo/pull/23756 (not yet in official emscripten)
- Workaround: Use explicit full paths instead of `FS.Absolute` in wasm builds
- Example in `general.shs`:
  ```shards
  #("." | FS.Absolute)
  If({@platform | Is("emscripten")} {
    "/tfs/shards/tests/"  // workaround: explicit path
  } Pass) = test-path
  ```

**wasmfs fetch backend is read-only:**
- Files under the HTTP-mounted `/tfs` directory cannot be written to
- Workaround: A writable memory backend is mounted at `/tmp` for tests that need to write files

### JSPI Fiber Implementation (Experimental)

The codebase includes an experimental JSPI (JavaScript Promise Integration) based fiber implementation as an alternative to Asyncify. JSPI is a WebAssembly standard that allows Wasm to suspend and resume execution without the binary instrumentation overhead of Asyncify.

**Status: NOT WORKING** - JSPI conflicts with pthreads during static initialization. See https://github.com/emscripten-core/emscripten/issues/19287

**Files:**
- `shards/core/shards_fiber.js` - JS fiber manager with manual stack save/restore
- `shards/core/coro.hpp` / `coro.cpp` - JSPI Fiber struct (conditional on `SHARDS_USE_JSPI`)
- `shards/core/CMakeLists.txt` - JSPI build flags (`-sJSPI=1`, `JSPI_IMPORTS`, `JSPI_EXPORTS`)

**Just Commands (experimental):**
- `just configure-wasm-jspi` - Configure cmake for JSPI wasm build
- `just build-wasm-jspi` - Build with JSPI (will compile but fail at runtime)
- `just test-wasm-jspi` - Run tests with JSPI build

**The Problem:**
JSPI and pthreads have compatibility issues. During C++ static initialization, pthread mutex operations trigger JSPI suspension, but this happens before any `WebAssembly.promising` context exists, causing:
```
Error: trying to suspend without WebAssembly.promising
```

**Why We Can't Disable Pthreads:**
The codebase depends on `boost::thread` which requires pthreads. Disabling pthreads causes compilation failures.

**When This Might Work:**
- When Emscripten fixes JSPI+pthreads compatibility
- Requires Chrome 137+ or Firefox 139+ for JSPI support

**Key Insight (Shadow Stack Problem):**
JSPI only handles the native Wasm stack. The linear memory stack (where C++ variables live) must be manually saved/restored. The implementation in `shards_fiber.js` handles this by copying the stack region to a buffer on suspend and restoring it on resume.
