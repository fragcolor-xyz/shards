# CMake Notes

## Rust.cmake - BINDGEN_EXTRA_CLANG_ARGS

When setting `BINDGEN_EXTRA_CLANG_ARGS` for non-Zig builds, use the CMake list directly:

```cmake
# CORRECT - use CMake list syntax directly
set(BINDGEN_EXTRA_CLANG_ARGS BINDGEN_EXTRA_CLANG_ARGS="${EXTRA_CLANG_ARGS}")

# WRONG - do not convert to space-separated string
list(JOIN EXTRA_CLANG_ARGS " " EXTRA_CLANG_ARGS_STR)
set(BINDGEN_EXTRA_CLANG_ARGS BINDGEN_EXTRA_CLANG_ARGS="${EXTRA_CLANG_ARGS_STR}")
```

The semicolon-separated CMake list format is expected by the build system. Converting to spaces breaks emscripten/wasm builds.

Note: For Zig cross-compilation, `BINDGEN_EXTRA_CLANG_ARGS` is passed via `_RUST_ENVIRONMENT` with a manually constructed space-separated string, which is correct for that code path.

## Rust.cmake - Emscripten Sysroot

For emscripten builds, bindgen needs a sysroot to find C++ headers like `<cmath>`. The sysroot is auto-detected from `CMAKE_INSTALL_PREFIX` which emscripten sets to its cache/sysroot directory. If this breaks, check:

1. `CMAKE_INSTALL_PREFIX` should point to `.../emscripten/cache/sysroot`
2. That directory should contain an `include/` folder with standard headers
3. The `EXTRA_CLANG_ARGS` list should include `--sysroot=...` and `-isystem.../include/compat`
