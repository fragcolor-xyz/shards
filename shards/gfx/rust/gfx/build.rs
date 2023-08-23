use std::env;

use cbindgen::Config;

fn main() {
  let crate_dir = env::var("CARGO_MANIFEST_DIR").unwrap();

  cbindgen::Builder::new()
    .with_config(Config {
      macro_expansion: cbindgen::MacroExpansionConfig { bitflags: true },
      structure: cbindgen::StructConfig {
        ..Default::default()
      },
      ..Default::default()
    })
    .with_namespace("gfx")
    .with_include_guard("SHARDS_GFX_RUST_BINDINGS_HPP")
    .with_sys_include("wgpu.h")
    .exclude_item("shardsInterface")
    .with_documentation(true)
    .with_crate(crate_dir)
    .generate()
    .expect("Unable to generate bindings")
    .write_to_file("bindings.hpp");
}
