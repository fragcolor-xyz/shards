#[macro_use]
extern crate shards;
#[macro_use]
extern crate lazy_static;

use cozo::*;
use shards::{SHCore, core::Core};

fn test_cozo() {
  let db = DbInstance::new("sqlite", "cozo.db", Default::default()).unwrap();
  let script = "?[a] := a in [1, 2, 3]";
  let result = db
    .run_script(script, Default::default(), ScriptMutability::Immutable)
    .unwrap();
  println!("test_cozo: {:?}", result);
}

#[no_mangle]
pub extern "C" fn shardsRegister_sqlite_rust(core: *mut SHCore) {
  unsafe {
    Core = core;
  }

  test_cozo();
}
