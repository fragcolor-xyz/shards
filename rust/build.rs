/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2020 Fragcolor Pte. Ltd. */

#[cfg(feature = "run_bindgen")]
extern crate bindgen;

#[cfg(feature = "run_bindgen")]
fn bindgen_it() {
  use std::env::var;

  let shards_dir = var("SHARDS_DIR").unwrap_or("../".to_string());

  println!("cargo:rustc-link-search={}/build", shards_dir);
  // Tell cargo to invalidate the built crate whenever the wrapper changes
  println!(
    "cargo:rerun-if-changed={}/include/shards.h",
    shards_dir
  );

  let header_path = shards_dir + "/include/shards.h";

  let bindings = bindgen::Builder::default()
    .header(header_path)
    .clang_arg("-DSH_NO_ANON")
    .clang_arg("-DSH_USE_ENUMS")
    .derive_default(true)
    .use_core()
    .generate()
    .expect("Unable to generate bindings");

  bindings
    .write_to_file("src/shardsc.rs")
    .expect("Couldn't write bindings!");

  println!("Done processing shards.h");
}

#[cfg(not(feature = "run_bindgen"))]
fn bindgen_it() {}

fn main() {
  bindgen_it();
}
