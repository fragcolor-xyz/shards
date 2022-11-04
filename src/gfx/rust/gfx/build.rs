use std::env;
use std::path::PathBuf;

fn main() {
    println!("cargo:rerun-if-changed=ffi/gfx_rust.h");
    let wgpu_ffi_path = "../wgpu-native/ffi";

    // println!("cargo:rerun-if-changed={}/wgpu.h", wgpu_ffi_path);
    // println!(
    //     "cargo:rerun-if-changed={}/webgpu-headers/webgpu.h",
    //     wgpu_ffi_path
    // );

    let builder = bindgen::Builder::default()
        .clang_arg(format!("-I{}", wgpu_ffi_path))
        .clang_arg(format!("-I{}/webgpu-headers", wgpu_ffi_path))
        .header("ffi/gfx_rust.h")
        .parse_callbacks(Box::new(bindgen::CargoCallbacks))
        .blocklist_type("(^WGPUProc).*")
        .blocklist_function("wgpuGetProcAddress")
        .prepend_enum_name(false)
        .size_t_is_usize(true)
        .ignore_functions()
        .layout_tests(false)
        .rustfmt_bindings(true);

    let bindings = builder.generate().expect("Unable to generate bindings");
    let out_path = PathBuf::from(env::var("OUT_DIR").unwrap());
    bindings
        .write_to_file(out_path.join("bindings.rs"))
        .expect("Couldn't write bindings!");
}
