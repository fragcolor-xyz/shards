// extern crate shards;

use egui::{self, Ui, *};
use shards::shards::gui::util;
use shards::types::Var;

pub mod native {
    #![allow(non_upper_case_globals)]
    #![allow(non_camel_case_types)]
    #![allow(non_snake_case)]
    #![allow(dead_code)]

    include!(concat!(env!("OUT_DIR"), "/bindings.rs"));
}

#[no_mangle]
unsafe extern "C" fn showInputDebugUI(
    context_var: &Var,
    layers_ptr: *const native::shards_input_debug_Layer,
    num_layers: usize,
) {
    let layers = std::slice::from_raw_parts(layers_ptr, num_layers);

    let gui_ctx =
        util::get_current_context_from_var(context_var).expect("Failed to get the UI context");
    Window::new("Input").show(gui_ctx, |ui| {
        ui.label("Hello World!");
    });
}
