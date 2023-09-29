// extern crate shards;

use egui::{self, *};
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
pub unsafe extern "C" fn shards_input_showDebugUI(
  context_var: &Var,
  layers_ptr: *const native::shards_input_debug_Layer,
  num_layers: usize,
) {
  let layers = std::slice::from_raw_parts(layers_ptr, num_layers);

  let egui_ctx = &util::get_current_context_from_var(context_var)
    .expect("Failed to get the UI context")
    .egui_ctx;
  Window::new("Input").show(egui_ctx, |ui| {
    for (i, layer) in layers.iter().enumerate() {
      ui.push_id(i, |ui| {
        ui.horizontal(|ui| {
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
          ui.colored_label(c, format!("layer: {} ", str));

          let consume_flags = &layer.consumeFlags;
          if consume_flags.requestFocus {
            ui.colored_label(epaint::Color32::LIGHT_BLUE, "[Focus] ");
          }
          if consume_flags.wantsPointerInput {
            ui.colored_label(epaint::Color32::LIGHT_YELLOW, "[Pointer] ");
          }
          if consume_flags.wantsKeyboardInput {
            ui.colored_label(epaint::Color32::LIGHT_RED, "[Keyboard] ");
          }
          if consume_flags.canReceiveInput {
            ui.colored_label(epaint::Color32::GREEN, "[Receive] ");
          }
        });
        ui.end_row();

        let events = std::slice::from_raw_parts(layer.debugEvents, layer.numDebugEvents);

        let text_style = TextStyle::Small;
        let row_height = ui.text_style_height(&text_style);

        ui.end_row();

        ui.allocate_ui([ui.available_size().x, 64.0].into(), |ui| {
          ScrollArea::vertical()
            .stick_to_bottom(true)
            .auto_shrink([false, false])
            .show_rows(ui, row_height, events.len(), |ui, row_range| {
              for row in row_range {
                let c = if native::shards_input_eventIsConsumed(events[row]) {
                  epaint::Color32::RED
                } else {
                  epaint::Color32::WHITE
                };

                let cstr = native::shards_input_eventToString(events[row]);
                let str = std::ffi::CStr::from_ptr(cstr).to_string_lossy();
                ui.colored_label(c, format!("event: {}", str));
                native::shards_input_freeString(cstr);
                ui.end_row();
              }
            });
        });
        ui.end_row();
      });
      ui.end_row();
    }
  });
}
