/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2020 Fragcolor Pte. Ltd. */

extern crate bindgen;
extern crate gfx_build;
use std::env;
use std::path::PathBuf;

fn generate_shardsc() {
  use std::env::var;

  let shards_dir = var("SHARDS_DIR").unwrap_or("..".to_string());
  let shards_include_dir = format!("{}/include", shards_dir);
  let src_extra_dir = format!("{}/src/extra", shards_dir);
  let gfx_path = format!("{}/src/gfx", shards_dir);

  let main_header_path = format!("{}/rust_interop.hpp", src_extra_dir);

  // Tell cargo to regenerate the bindings whenever the headers change
  println!("cargo:rerun-if-changed={}/shards.h", shards_include_dir);
  println!("cargo:rerun-if-changed={}", main_header_path);

  let builder = gfx_build::setup_bindgen_for_gfx(gfx_path.as_str(), bindgen::Builder::default());
  let bindings = builder
    .header(main_header_path)
    .clang_arg("-DSH_NO_ANON")
    .clang_arg("-DSH_USE_ENUMS")
    .allowlist_type("SH.*")
    .allowlist_var("SH.*")
    .allowlist_function("SH.*")
    .clang_arg(format!("-I{}", shards_include_dir))
    .clang_arg(format!("-I{}/src", shards_dir))
    .derive_default(true)
    .use_core()
    .generate()
    .expect("Unable to generate bindings");

  let out_path = PathBuf::from(env::var("OUT_DIR").unwrap());
  bindings
    .write_to_file(out_path.join("shardsc.rs"))
    .expect("Couldn't write bindings!");

  println!("Done processing shards.h");
}

fn main() {
  generate_shardsc();
}
