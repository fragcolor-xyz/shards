use std::env;

extern crate bindgen;

pub fn setup_bindgen_for_gfx(gfx_path: &str, builder: bindgen::Builder) -> bindgen::Builder {
  let deps_path = format!("{}/../../deps", gfx_path);

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
    .clang_arg(format!("-I{}", deps_path))
    .clang_arg(format!("-I{}/linalg", deps_path))
    .clang_arg(format!("-I{}/nameof/include", deps_path))
    .clang_arg(format!("-I{}/spdlog/include", deps_path))
    .clang_arg(format!("-I{}/SDL3/include", deps_path))
    .clang_arg(format!("-I{}/rust/wgpu-native/ffi", gfx_path))
    .clang_arg(format!("-I{}/rust/wgpu-native/ffi/webgpu-headers", gfx_path))
    .clang_arg("-std=c++17")
    .rust_target(bindgen::RustTarget::Nightly) // Required for thiscall on x86 windows
    .size_t_is_usize(true);

  let target_env = env::var("CARGO_CFG_TARGET_ARCH").unwrap();
  if target_env != "wasm32" {
    builder = builder.clang_arg(format!("-DWEBGPU_NATIVE=1"));
  }

  builder
}
