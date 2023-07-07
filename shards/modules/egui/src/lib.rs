use shards_egui;

#[no_mangle]
pub extern "C" fn shardsRegister_egui_egui(core: *mut shards::shardsc::SHCore) {
  shards_egui::register(core); 
}
