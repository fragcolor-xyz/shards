use std::env;

extern crate bindgen;

pub fn setup_bindgen_for_gfx(gfx_path: &str, builder: bindgen::Builder) -> bindgen::Builder {
  let deps_path = format!("{}/../../deps", gfx_path);

  // Check for CPM _deps directory (from CMAKE_BINARY_DIR env var or OUT_DIR)
  let cpm_deps_path = env::var("CMAKE_BINARY_DIR")
    .or_else(|_| {
      // Try to find build directory from OUT_DIR
      env::var("OUT_DIR").map(|out| {
        // OUT_DIR is typically something like: /path/to/build/target/.../build/crate-hash/out
        // We need to find the build directory
        let mut path = std::path::PathBuf::from(&out);
        // Go up until we find a directory that contains "_deps"
        while path.pop() {
          let deps_candidate = path.join("_deps");
          if deps_candidate.exists() {
            return path.to_string_lossy().to_string();
          }
        }
        String::new()
      })
    })
    .ok()
    .and_then(|build_dir| {
      if !build_dir.is_empty() {
        Some(format!("{}/_deps", build_dir))
      } else {
        None
      }
    });

  println!("cargo:rerun-if-changed={}/rust_interop.hpp", gfx_path);

  let mut builder = builder
    .allowlist_function("gfx::.*")
    .allowlist_function("gfx_.*")
    .allowlist_type("gfx::.*")
    .blocklist_type("gfx::shader::.*")
    .blocklist_function("gfx::shader::.*")
    .blocklist_item("gfx::shader::.*")
    .blocklist_type("__gnu_cxx::new.*")
    .blocklist_type("std::__shared_count")
    .blocklist_type("std::.*")
    .opaque_type("std::.*")
    .clang_arg("-DRUST_BINDGEN=1")
    .clang_arg(format!("-I{}/..", gfx_path))
    .clang_arg(format!("-I{}", deps_path));

  let target_arch = env::var("CARGO_CFG_TARGET_ARCH").unwrap();

  // Add CPM dependencies if available
  if let Some(ref cpm_deps) = cpm_deps_path {
    builder = builder
      .clang_arg(format!("-I{}", cpm_deps))
      .clang_arg(format!("-I{}/linalg-src", cpm_deps))
      .clang_arg(format!("-I{}/sdl3-src/include", cpm_deps));

    // SDL3 build directory only exists for non-wasm32 targets (where SDL3 is actually built)
    if target_arch != "wasm32" {
      builder = builder
        .clang_arg(format!("-I{}/sdl3-build/include", cpm_deps));
    }
  } else {
    // Fall back to old submodule locations
    builder = builder
      .clang_arg(format!("-I{}/linalg", deps_path))
      .clang_arg(format!("-I{}/SDL3/include", deps_path));
  }

  // Common dependencies (always in deps/)
  builder = builder
    .clang_arg(format!("-I{}/nameof/include", deps_path))
    .clang_arg(format!("-I{}/spdlog/include", deps_path))
    .clang_arg(format!("-I{}/rust/wgpu-native/ffi", gfx_path))
    .clang_arg(format!("-I{}/rust/wgpu-native/ffi/webgpu-headers", gfx_path))
    .clang_arg("-std=c++17")
    .rust_target(bindgen::RustTarget::Nightly) // Required for thiscall on x86 windows
    .size_t_is_usize(true);

  if target_arch != "wasm32" {
    builder = builder.clang_arg(format!("-DWEBGPU_NATIVE=1"));
  }

  builder
}
