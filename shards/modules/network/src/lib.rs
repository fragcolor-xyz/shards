
#[macro_use]
extern crate shards;

#[macro_use]
extern crate lazy_static;

#[macro_use]
extern crate compile_time_crc32;

pub mod ws;

#[no_mangle]
pub extern "C" fn shardsRegister_network_rust(core: *mut shards::shardsc::SHCore) {
unsafe {
  shards::core::Core = core;
  }

  ws::register_shards();
}