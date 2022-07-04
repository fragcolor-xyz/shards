extern crate bindgen;
extern crate gfx_build;

use std::env;
use std::path::PathBuf;

fn main() {
    let gfx_path = "..".to_string();

    let builder = gfx_build::setup_bindgen_for_gfx(gfx_path.as_str(), bindgen::Builder::default());

    let bindings = builder
        .header("rust_interop.hpp")
        .size_t_is_usize(true)
        .layout_tests(false)
        .generate()
        .expect("Unable to generate bindings");

    let out_path = PathBuf::from(env::var("OUT_DIR").unwrap());
    bindings
        .write_to_file(out_path.join("bindings.rs"))
        .expect("Couldn't write bindings!");
}
