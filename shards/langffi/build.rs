use std::env;

fn main() {
  let crate_dir = env::var("CARGO_MANIFEST_DIR").unwrap();

  cbindgen::Builder::new()
    .with_pragma_once(true)
    .exclude_item("shardsInterface")
    .exclude_item("shardsRegister_lang_lang")
    .exclude_item("DebugBreak")
    .with_documentation(true)
    .with_after_include("struct Program;\nstruct Sequence;\nstruct EvalEnv;")
    .rename_item("Var", "SHVar")
    .rename_item("Wire", "SHWireRef")
    .with_sys_include("shards/core/runtime.hpp")
    // .with_after_include("using namespace shards;\nstruct FormMapUpdater;")
    // .with_sys_include("shards/core/runtime.hpp")
    .with_crate(crate_dir)
    .generate()
    .expect("Unable to generate bindings")
    .write_to_file("bindings.h");
}
