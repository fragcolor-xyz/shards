/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2020 Fragcolor Pte. Ltd. */

extern crate bindgen;
use std::env;
use std::path::PathBuf;

fn generate_shardsc() {
  let shards_dir = "../..";
  let shards_include_dir = format!("{}/include", shards_dir);
  let main_header_path = format!("{}/shards/shards.h", shards_include_dir);

  // Tell cargo to regenerate the bindings whenever the headers change
  println!("cargo:rerun-if-changed={}", main_header_path);

  let builder = bindgen::Builder::default();
  let bindings = builder
    .header(main_header_path)
    .clang_arg("-DSH_NO_ANON")
    .clang_arg("-DSH_USE_ENUMS")
    .clang_arg("-DRUST_BINDGEN")
    .allowlist_type("SHImage")
    .allowlist_type("SHImage")
    .allowlist_var("SHImage")
    .allowlist_type("SH.*")
    .allowlist_var("SH.*")
    .allowlist_function("SH.*")
    .allowlist_function("util_.*")
    .clang_arg(format!("-I{}", shards_include_dir))
    .clang_arg(format!("-I{}/shards/core", shards_dir))
    .derive_default(true)
    .layout_tests(false)
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
