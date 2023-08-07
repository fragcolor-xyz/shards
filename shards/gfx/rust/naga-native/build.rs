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
    .with_namespace("naga")
    .with_include_guard("NAGA_NATIVE_BINDINGS_HPP")
    .with_after_include(
      r"
namespace naga {
struct Statement;
struct Constant;
struct Type;
struct Expression;

// Manually defined first to fix cyclic dependency issues
struct Block {
  const Statement *body;
  uint32_t body_len;
};
}",
    )
    .exclude_item("Block")
    .include_item("Statement")
    .with_documentation(true)
    .with_crate(crate_dir)
    .generate()
    .expect("Unable to generate bindings")
    .write_to_file("bindings.hpp");
}
