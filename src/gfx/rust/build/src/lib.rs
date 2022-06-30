extern crate bindgen;

pub fn setup_bindgen_for_gfx(gfx_path: &str, builder: bindgen::Builder) -> bindgen::Builder {
    let egui_path = format!("{}/egui", gfx_path);
    let deps_path = format!("{}/../../deps", gfx_path);

    println!("cargo:rerun-if-changed={}/rust_interop.hpp", gfx_path);
    println!("cargo:rerun-if-changed={}/rust_interop.hpp", egui_path);
    println!("cargo:rerun-if-changed={}/renderer.hpp", egui_path);
    println!("cargo:rerun-if-changed={}/egui_types.hpp", egui_path);

    builder
        .allowlist_function("gfx::.*")
        .allowlist_function("gfx_.*")
        .allowlist_type("gfx::.*")
        .blocklist_type("std::.*")
        .blocklist_type("__gnu_cxx::new.*")
        .opaque_type("std::shared_ptr.*")
        .opaque_type("std::weak_ptr.*")
        .opaque_type("std::unique_ptr.*")
        .opaque_type("std::vector.*")
        .opaque_type("std::string.*")
        .clang_arg("-DRUST_BINDGEN=1")
        .clang_arg("-DWEBGPU_NATIVE=1")
        .clang_arg(format!("-I{}", deps_path))
        .clang_arg(format!("-I{}/nameof/include", deps_path))
        .clang_arg(format!("-I{}/wgpu-native/ffi", gfx_path))
        .clang_arg("-std=gnu++20")
        .clang_arg("-v")
        .size_t_is_usize(true)
}
