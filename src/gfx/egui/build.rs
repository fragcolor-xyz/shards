extern crate bindgen;

use std::env;
use std::path::PathBuf;

fn main() {
    println!("cargo:rerun-if-changed=egui_interop.hpp");

    let bindings = bindgen::Builder::default()
        .blocklist_item("std::.*")
        .clang_arg("-DRUST_BINDGEN=1")
        .clang_arg("-std=c++17")
        .header("egui_interop.hpp")
        .size_t_is_usize(true)
        .generate()
        .expect("Unable to generate bindings");

    let out_path = PathBuf::from(env::var("OUT_DIR").unwrap());
    bindings
        .write_to_file(out_path.join("bindings.rs"))
        .expect("Couldn't write bindings!");
}
