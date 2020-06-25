extern crate bindgen;

fn main() {
    println!("cargo:rustc-link-search=../chainblocks/build");
    
    // Tell cargo to invalidate the built crate whenever the wrapper changes
    println!("cargo:rerun-if-changed=../chainblocks/include/chainblocks.h");

    let bindings = bindgen::Builder::default()
        .header("../chainblocks/include/chainblocks.h")
        .clang_arg("-I../chainblocks/deps/stb")
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
