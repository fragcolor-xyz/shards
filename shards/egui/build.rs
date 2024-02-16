use bindgen;
use gfx_build;

use std::env;
use std::path::PathBuf;

fn main() {
  let gfx_path = "../gfx";
  let shards_root = "../..";

  println!("cargo:rerun-if-changed=rust_interop.hpp");
  println!("cargo:rerun-if-changed=renderer.hpp");
  println!("cargo:rerun-if-changed=egui_types.hpp"); 
  println!("cargo:rerun-if-changed=input.hpp");
  println!("cargo:rerun-if-changed=context.hpp");

  let mut builder = gfx_build::setup_bindgen_for_gfx(gfx_path, bindgen::Builder::default());
  builder = builder
    .header("rust_interop.hpp")
    .allowlist_type("egui::.*")
    .allowlist_var("SDLK_.*")
    .allowlist_type("SDL_KeyCode")
    .size_t_is_usize(true)
    .layout_tests(false)
    .clang_arg(format!("-I{}/include", shards_root));

  let bindings = builder.generate().expect("Unable to generate bindings");

  let out_path = PathBuf::from(env::var("OUT_DIR").unwrap());
  bindings
    .write_to_file(out_path.join("bindings.rs"))
    .expect("Couldn't write bindings!");
}
