# GFX Module - WebGPU Integration

## Architecture Overview

The GFX module wraps WebGPU (wgpu) for cross-platform graphics. It uses **custom forks** of wgpu and wgpu-native maintained in the `shards-lang` GitHub organization.

## Directory Structure

```
shards/gfx/
├── rust/
│   ├── wgpu/           # Fork: shards-lang/wgpu (submodule)
│   ├── wgpu-native/    # Fork: shards-lang/wgpu-native (submodule)
│   ├── naga-native/    # Custom naga FFI bindings
│   ├── gfx/            # Main gfx crate (separate workspace!)
│   └── profiling/      # Tracy profiling fork
├── *.cpp/*.hpp         # C++ graphics code
└── CLAUDE.md           # This file
```

## Fork Branches

| Repository | Branch | Base Version |
|------------|--------|--------------|
| shards-lang/wgpu | `shards-27.x` | v27.0.4 |
| shards-lang/wgpu-native | `shards-27.x` | v27.0.2.0 |

## Custom Patches (Must Preserve on Upgrades)

### wgpu fork (`shards-27.x`)
1. **Naga Handle exposure** - Makes `Handle::new`, `from_usize`, `from_usize_unchecked` public and exports `Index` type. Required by naga-native.
2. **objc 0.2.7 compatibility** - Removed direct `sel_impl` imports (use `sel!` macro instead).

### wgpu-native fork (`shards-27.x`)
1. **Type exposure** - Makes struct fields `pub` for: `QueueId`, `WGPUDeviceImpl`, `WGPUInstanceImpl`, `WGPUPipelineLayoutImpl`, `QuerySetData`, `WGPUQuerySetImpl`, `WGPUQueueImpl`, `WGPURenderBundleImpl`, `WGPURenderBundleEncoderImpl`, `WGPUSurfaceImpl`, `TextureData`, `WGPUTextureImpl`, `WGPUTextureViewImpl`, `ErrorSink`, `ErrorSinkRaw`, `WGPUShaderModuleImpl`.
2. **Cargo.toml** - Uses `crate-type = ["lib"]` (not cdylib/staticlib) and path dependencies to local wgpu.
3. **webgpu-headers** - Submodule must match the wgpu-native version.

## Required Cargo Patches

### Why objc fork is needed
The crates.io `objc 0.2.7` has broken macro exports - `sel_impl!` isn't properly exported for `sel!` and `msg_send!` macro expansion. The shards fork fixes this.

### Patch locations (BOTH required!)
1. **Root `/Cargo.toml`** - For main workspace
2. **`shards/gfx/rust/gfx/Cargo.toml`** - Has `[workspace]`, built separately

```toml
[patch.crates-io]
objc = { git = "https://github.com/shards-lang/rust-objc.git", branch = "shards-0.2.7" }
naga = { path = "../wgpu/naga" }
```

## WebGPU v27 API Notes

### String handling
All labels and entry points now use `WGPUStringView` instead of `const char*`:
```cpp
// Use helper from gfx_wgpu.hpp
desc.label = wgpuMakeStringView("my_label");
vertex.entryPoint = wgpuMakeStringView("vertex_main");
```

### Callback signatures
Callbacks now use info structs with two userdata pointers:
```cpp
WGPURequestAdapterCallbackInfo callbackInfo{
    .mode = WGPUCallbackMode_AllowSpontaneous,
    .callback = myCallback,
    .userdata1 = myData,
    .userdata2 = nullptr,
};
wgpuInstanceRequestAdapter(instance, &options, callbackInfo);

// Callback signature:
void myCallback(WGPURequestAdapterStatus status, WGPUAdapter adapter,
                WGPUStringView message, void* userdata1, void* userdata2);
```

### Type renames (v22 -> v27)
| Old | New |
|-----|-----|
| `WGPUImageCopyTexture` | `WGPUTexelCopyTextureInfo` |
| `WGPUImageCopyBuffer` | `WGPUTexelCopyBufferInfo` |
| `WGPUTextureDataLayout` | `WGPUTexelCopyBufferLayout` |
| `WGPUSurfaceDescriptorFrom*` | `WGPUSurfaceSource*` |
| `WGPUSupportedLimits` | `WGPULimits` |
| `WGPURequiredLimits` | `WGPULimits` (pass directly) |
| `WGPUBufferMapAsyncStatus` | `WGPUMapAsyncStatus` |
| `WGPUShaderModuleWGSLDescriptor` | `WGPUShaderSourceWGSL` |

### Limits changes
- `WGPULimits` used directly (no wrapper struct)
- Native limits via `WGPUNativeLimits` chain extension
- `maxInterStageShaderComponents` removed (only `maxInterStageShaderVariables`)

### Depth state
`depthWriteEnabled` is now `WGPUOptionalBool` enum:
```cpp
depthStencilState.depthWriteEnabled = enabled ? WGPUOptionalBool_True : WGPUOptionalBool_False;
```

### Surface texture status
```cpp
// Old: WGPUSurfaceGetCurrentTextureStatus_Success
// New: Check both optimal and suboptimal
if (st.status != WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal &&
    st.status != WGPUSurfaceGetCurrentTextureStatus_SuccessSuboptimal) {
    // Error
}
```

### Global report (Tracy profiling)
```cpp
// Old: Per-backend reports
WGPUHubReport *hubReport = &report.vulkan; // or .dx12, .metal

// New: Single hub report
WGPUHubReport *hubReport = &report.hub;
```

## Upgrading wgpu

1. **Fetch upstream tags** in both forks
2. **Create new branch** from upstream tag (e.g., `shards-28.x` from `v28.0.0`)
3. **Cherry-pick patches** listed above (naga exposure, type exposure)
4. **Check upstream changelog** for API changes
5. **Update webgpu-headers** submodule in wgpu-native
6. **Update Cargo.toml** path dependencies
7. **Fix C++ code** for any API changes
8. **Test both debug AND release builds** (release compiles fresh, debug may use cache)

## Build Notes

- Debug build may use cached artifacts - always verify with release build
- The gfx crate is a **separate workspace** - patches must be in its Cargo.toml
- Use `just cargo-check` for Rust, `just build` / `just build-rel` for full builds
- **Test with Tracy**: CI uses `-DTRACY_ENABLE=ON`. Test locally with:
  ```bash
  cmake -S . -B build/DebugTracy -G Ninja -DCMAKE_BUILD_TYPE=Debug -DTRACY_ENABLE=ON
  cmake --build build/DebugTracy --target shards
  ```

## Key Files

| File | Purpose |
|------|---------|
| `gfx_wgpu.hpp` | WebGPU helpers, `wgpuMakeStringView` (includes `<cstring>` for `strlen`) |
| `context.cpp` | Device/adapter creation, callbacks |
| `platform_surface.hpp` | Platform-specific surface creation |
| `renderer.cpp` | Buffer mapping, texture copies |
| `rust/gfx/src/lib.rs` | Shader module creation FFI |
| `rust/naga-native/src/lib.rs` | Naga type conversions |
