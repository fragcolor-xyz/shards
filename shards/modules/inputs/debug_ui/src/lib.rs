// extern crate shards;

use std::ffi::CStr;

use egui::{self, *};
use shards::types::Var;
use shards_egui::util;
use shards::util::from_raw_parts_allow_null;

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
  params: *mut native::shards_input_debug_DebugUIParams,
) {
  let params = &mut (*params);
  let layers = from_raw_parts_allow_null(params.layers, params.numLayers);
  let events = from_raw_parts_allow_null(params.events, params.numEvents);

  let opts = &mut *params.opts;

  let egui_ctx = &util::get_current_context_from_var(context_var)
    .expect("Failed to get the UI context")
    .egui_ctx;
  Window::new("Input")
    .resizable(true)
    .min_width(400.0)
    .min_height(300.0)
    .show(egui_ctx, |ui| {
      // Show layers
      ui.label("Layers:");
      for (i, layer) in layers.iter().enumerate() {
        ui.push_id(i, |ui| {
          ui.horizontal(|ui| {
            let c = if layer.hasFocus {
              epaint::Color32::LIGHT_BLUE
            } else {
              epaint::Color32::WHITE
            };

            let str = if layer.name.is_null() {
              "<unnamed>".into()
            } else {
              std::ffi::CStr::from_ptr(layer.name).to_string_lossy()
            };
            ui.colored_label(c, format!("layer: {} ", str));

            if layer.hasFocus {
              ui.colored_label(epaint::Color32::LIGHT_BLUE, " [Focus]");
            }
          });
        });
      }

      let text_style = TextStyle::Small;
      let row_height = ui.text_style_height(&text_style);

      ui.label(format!("Current Frame: {}", params.currentFrame));
      ui.horizontal(|ui| {
        ui.label("Events: ");
        ui.checkbox(&mut opts.showPointerEvents, "Pointer");
        ui.add_enabled_ui(opts.showPointerEvents, |ui| {
          ui.checkbox(&mut opts.showPointerMoveEvents, "Pointer (move)");
        });
        ui.checkbox(&mut opts.showTouchEvents, "Touch");
        ui.checkbox(&mut opts.showKeyboardEvents, "Keyboard");
        if ui.button("Clear").clicked() {
          params.clearEvents = true;
        }
        ui.checkbox(&mut opts.freeze, "Freeze");
      });

      ui.allocate_ui(
        [ui.available_size().x, ui.available_size().y].into(),
        |ui| {
          ScrollArea::vertical()
            .stick_to_bottom(true)
            .auto_shrink([false, false])
            .show_rows(ui, row_height, events.len(), |ui, row_range| {
              for row in row_range {
                ui.horizontal(|ui| {
                  let event = &events[row];
                  let consumed_by = native::shards_input_eventConsumedBy(event.evt);
                  // let c = if native::shards_input_eventIsConsumed(event.evt) {
                  //   epaint::Color32::WHITE
                  // } else {
                  //   epaint::Color32::LIGHT_GRAY
                  // };

                  let _type = native::shards_input_eventType(event.evt);
                  let ecolor = match _type {
                    1 | 3 => epaint::Color32::LIGHT_BLUE,
                    5 => epaint::Color32::LIGHT_GREEN,
                    6 => epaint::Color32::from_rgb(0xb0, 0x5a, 0x24),
                    _ => epaint::Color32::GRAY,
                  };
                  // TODO: Bring back type colors
                  //   ui.colored_label(epaint::Color32::LIGHT_YELLOW, "[Pointer] ");
                  //   ui.colored_label(epaint::Color32::LIGHT_RED, "[Keyboard] ");

                  let cstr = native::shards_input_eventToString(event.evt);
                  let str = std::ffi::CStr::from_ptr(cstr).to_string_lossy();
                  ui.colored_label(ecolor, format!("{}: {}", event.frameIndex, str));
                  native::shards_input_freeString(cstr);

                  if consumed_by != std::ptr::null_mut() {
                    let ptr = native::shards_input_layerName(consumed_by);
                    let name = CStr::from_ptr(ptr).to_str().unwrap();
                    ui.colored_label(epaint::Color32::WHITE, format!(" [{}]", name));
                    native::shards_input_freeString(ptr);
                  }
                });
                ui.end_row();
              }
            });
        },
      );
    });
}
