# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build and Development Commands

### Essential Commands
- **Bootstrap**: `./bootstrap` - Initialize dependencies and tools (run first)
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
1. `./bootstrap` - Sets up all dependencies and tools
2. `just build` - Creates initial debug build
3. `just tests` - Verify installation works

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
