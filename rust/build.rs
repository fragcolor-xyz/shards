extern crate bindgen;
use std::env::var;

fn main() {
    let chainblocks_dir = var("CHAINBLOCKS_DIR").unwrap_or("../".to_string());

    println!("cargo:rustc-link-search={}/build", chainblocks_dir);
    // Tell cargo to invalidate the built crate whenever the wrapper changes
    println!(
        "cargo:rerun-if-changed={}/include/chainblocks.h",
        chainblocks_dir
    );

    let header_path = chainblocks_dir + "/include/chainblocks.h";

    let bindings = bindgen::Builder::default()
        .header(header_path)
        .clang_arg("-DCB_NO_ANON")
        .clang_arg("-DCB_USE_ENUMS")
        .derive_default(true)
        .generate()
        .expect("Unable to generate bindings");

    bindings
        .write_to_file("src/chainblocksc.rs")
        .expect("Couldn't write bindings!");

    println!("Done processing chainblocks.h");
}
