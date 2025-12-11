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
