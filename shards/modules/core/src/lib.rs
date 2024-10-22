#[macro_use]
extern crate shards;

#[macro_use]
extern crate lazy_static;

pub mod casting;
pub mod date;
pub mod env;
pub mod uuid;

#[cfg(feature = "ffi")]
pub mod ffi;

pub mod yaml;

#[no_mangle]
pub extern "C" fn shardsRegister_core_rust(core: *mut shards::shardsc::SHCore) {
  unsafe {
    shards::core::Core = core;
  }

  date::register_shards();
  casting::register_shards();
  uuid::register_shards();
  env::register_shards();

  #[cfg(feature = "ffi")]
  {
    ffi::register_shards();
  }

  yaml::register_shards();
}
