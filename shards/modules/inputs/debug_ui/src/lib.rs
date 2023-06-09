// extern crate shards;

use egui::{self, Ui, *};
use shards::types::Var;
use shards_egui::util;

pub mod native {
  #![allow(non_upper_case_globals)]
  #![allow(non_camel_case_types)]
  #![allow(non_snake_case)]
  #![allow(dead_code)]

  include!(concat!(env!("OUT_DIR"), "/bindings.rs"));
}

#[no_mangle]
pub unsafe extern "C" fn showInputDebugUI(
  context_var: &Var,
  layers_ptr: *const native::shards_input_debug_Layer,
  num_layers: usize,
) {
  let layers = std::slice::from_raw_parts(layers_ptr, num_layers);

  let gui_ctx =
    util::get_current_context_from_var(context_var).expect("Failed to get the UI context");
  Window::new("Input").show(gui_ctx, |ui| {
    egui::Grid::new("my_grid")
      .num_columns(2)
      .spacing([40.0, 4.0])
      .striped(true)
      .show(ui, |ui| {
        for layer in layers {
          let c = if layer.focused {
            epaint::Color32::RED
          } else {
            epaint::Color32::WHITE
          };

          let str = if layer.name.is_null() {
            "<unnamed>".into()
          } else {
            std::ffi::CStr::from_ptr(layer.name).to_string_lossy()
          };
          ui.colored_label(c, format!("layer: {}", str));
          ui.end_row();
          // ui.label("Hello World!");
        }
      });
  });
}
