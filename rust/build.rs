extern crate bindgen;
extern crate cmake;

use cmake::Config;
use std::fs::{self, DirEntry};
use std::io;
use std::path::Path;

// one possible implementation of walking a directory only visiting files
fn visit_dirs<F>(dir: &Path, cb: F) -> io::Result<()>
where
  F: Fn(&DirEntry) + Copy,
{
  if dir.is_dir() {
    for entry in fs::read_dir(dir)? {
      let entry = entry?;
      let path = entry.path();
      if path.is_dir() {
        visit_dirs(&path, cb)?;
      } else {
        cb(&entry);
      }
    }
  }
  Ok(())
}

fn main() {
  // make sure to monitor c++ changes
  visit_dirs(Path::new("../src"), |x: &DirEntry| {
    println!("cargo:rerun-if-changed={}", x.path().to_str().unwrap())
  })
  .unwrap();
  visit_dirs(Path::new("../include"), |x: &DirEntry| {
    println!("cargo:rerun-if-changed={}", x.path().to_str().unwrap())
  })
  .unwrap();

  let cb_static = Config::new("../")
    .generator("Ninja")
    .build_target("format")
    .build_target("cb_static")
    .build();

  // linker search path
  println!("cargo:rustc-link-search=native={}/build", cb_static.display());
  println!("cargo:rustc-link-lib=static=cb_static");

  let header_path = "../include/chainblocks.h";

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
