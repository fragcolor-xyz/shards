extern crate bindgen;
extern crate gfx_build;

use std::env;
use std::path::PathBuf;

fn main() {
  let shards_dir = "../../../..";
  let gfx_path = "../../../gfx";
  let main_header_path = "../debug_ui.hpp";

  // Tell cargo to regenerate the bindings whenever the headers change
  println!("cargo:rerun-if-changed={}", main_header_path);

  let builder = gfx_build::setup_bindgen_for_gfx(gfx_path, bindgen::Builder::default());
  let bindings = builder
    .header(main_header_path)
    .clang_arg(format!("-I{}", shards_dir))
    .allowlist_type("egui::.*")
    .allowlist_type("shards::.*")
    .allowlist_var("SDLK_.*")
    .allowlist_type("SDL_Keycode")
    .allowlist_function("shards_input_.*")
    .opaque_type("std::.*")
    .size_t_is_usize(true)
    .layout_tests(false)
    .generate()
    .expect("Unable to generate bindings");

  let out_path = PathBuf::from(env::var("OUT_DIR").unwrap());
  bindings
    .write_to_file(out_path.join("bindings.rs"))
    .expect("Couldn't write bindings!");
}
