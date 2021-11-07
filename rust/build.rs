/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2020 Fragcolor Pte. Ltd. */

#[cfg(feature = "cblisp")]
extern crate cmake;

#[cfg(feature = "run_bindgen")]
extern crate bindgen;

#[cfg(feature = "run_bindgen")]
fn bindgen_it() {
  use std::env::var;

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
    .use_core()
    .generate()
    .expect("Unable to generate bindings");

  bindings
    .write_to_file("src/chainblocksc.rs")
    .expect("Couldn't write bindings!");

  println!("Done processing chainblocks.h");
}

#[cfg(not(feature = "run_bindgen"))]
fn bindgen_it() {}

#[cfg(feature = "cblisp")]
fn build_all() {
  // Builds the project in the directory located in `libfoo`, installing it
  // into $OUT_DIR
  let dst = cmake::Config::new("../")
    .define("RUST_CBLISP", "1")
    .build_target("cb_static")
    .build();
  println!("cargo:rustc-link-search=native={}/build", dst.display());
  println!("cargo:rustc-link-lib=static=cb_static");
}

#[cfg(not(feature = "cblisp"))]
fn build_all() {}

fn main() {
  bindgen_it();
  build_all();
}
