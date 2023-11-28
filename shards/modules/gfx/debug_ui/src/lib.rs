// extern crate shards;

use std::ffi::CStr;

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

#[derive(Default)]
struct State {
  selected_frame_queue: Option<usize>,
}

#[no_mangle]
pub unsafe extern "C" fn shards_gfx_newDebugUI() -> *mut std::ffi::c_void {
  Box::into_raw(Box::new(State::default())) as *mut std::ffi::c_void
}

#[no_mangle]
pub unsafe extern "C" fn shards_gfx_freeDebugUI(state: *mut std::ffi::c_void) {
  drop(Box::from_raw(state as *mut State));
}

fn default_frame() -> egui::Frame {
  egui::Frame::default()
    .rounding(Rounding::same(1.0))
    .fill(Color32::BLACK)
}

#[no_mangle]
pub unsafe extern "C" fn shards_gfx_showDebugUI(
  context_var: &Var,
  params: *mut native::shards_gfx_debug_ui_UIParams,
  num_layers: usize,
) {
  let params = &mut (*params);
  let state = &mut *(params.state as *mut State);
  let opts = &mut *params.opts;
  // let events = std::slice::from_raw_parts(params.events, params.numEvents);

  let egui_ctx = &util::get_current_context_from_var(context_var)
    .expect("Failed to get the UI context")
    .egui_ctx;

  let frame_queues = std::slice::from_raw_parts(params.frameQueues, params.numFrameQueues);

  Window::new("GFX")
    .resizable(true)
    .min_width(400.0)
    .min_height(300.0)
    .show(egui_ctx, |ui| {
      ui.horizontal(|ui| {
        // Frame queues list
        ui.with_layout(Layout::default().with_cross_justify(true), |ui| {
          ui.set_width(200.0);
          ui.set_height(120.0);
          default_frame().show(ui, |ui| {
            ui.label(RichText::new("Render calls:").strong().color(Color32::GRAY));
            ScrollArea::both().show(ui, |ui| {
              for (idx, frame) in frame_queues.iter().enumerate() {
                let selected = if let Some(selected) = state.selected_frame_queue {
                  idx == selected
                } else {
                  false
                };

                let mut label = format!("Call {}", idx);
                let mut color = Color32::WHITE;
                if frame.numNodes == 0 {
                  label += " (empty)";
                  color = Color32::GRAY;
                }

                let resp =
                  egui::SelectableLabel::new(selected, RichText::new(label).color(color)).ui(ui);
                if resp.clicked() {
                  state.selected_frame_queue = Some(idx);
                }
              }
            });
          });
        });

        // Frames list
        ui.with_layout(Layout::default().with_cross_justify(true), |ui| {
          ui.set_width(200.0);
          ui.set_height(120.0);
          default_frame().show(ui, |ui| {
            if let Some(selected) = state.selected_frame_queue {
              if selected < frame_queues.len() {
                let fq = &frame_queues[selected];

                // Show frames
                egui::Frame::default().show(ui, |ui| {
                  ui.label(
                    RichText::new(format!("Frames:"))
                      .strong()
                      .color(Color32::GRAY),
                  );
                  ScrollArea::both().show(ui, |ui| {
                    let frames = std::slice::from_raw_parts(fq.frames, fq.numFrames);
                    for (idx, frame) in frames.iter().enumerate() {
                      let name = CStr::from_ptr(frame.name);
                      let mut title =
                        format!("Frame {}/{}:", idx, name.to_str().unwrap()).to_string();
                      if frame.isOutput {
                        title += " (output)";
                      }
                      ui.label(RichText::new(title).strong());
                      ui.indent(0, |ui| {
                        ui.label(
                          RichText::new(format!("{}x{}", frame.size.x, frame.size.y))
                            .color(Color32::GRAY),
                        );
                        ui.label(
                          RichText::new(format!("Format {}", frame.format)).color(Color32::GRAY),
                        );
                      });
                    }
                  });
                });
              }
            }
          });
        });
      });

      // for (idx, layer) in frameQueues[state.selected_frame_queue]..iter().enumerate() {
      //   let resp = egui::SelectableLabel::new(
      //     idx == state.selected_frame_queue,
      //     format!("Layer {}", idx),
      //   )
      //   .ui(ui);
      //   if resp.clicked() {
      //     state.selected_frame_queue = idx;
      //   }
      // }
      // state.
      // Show layers
      // ui.label("Layers:");
      // for (i, layer) in layers.iter().enumerate() {
      //   ui.push_id(i, |ui| {
      //     ui.horizontal(|ui| {
      //       let c = if layer.hasFocus {
      //         epaint::Color32::LIGHT_BLUE
      //       } else {
      //         epaint::Color32::WHITE
      //       };

      //       let str = if layer.name.is_null() {
      //         "<unnamed>".into()
      //       } else {
      //         std::ffi::CStr::from_ptr(layer.name).to_string_lossy()
      //       };
      //       ui.colored_label(c, format!("layer: {} ", str));

      //       if layer.hasFocus {
      //         ui.colored_label(epaint::Color32::LIGHT_BLUE, " [Focus]");
      //       }
      //     });
      //   });
      // }

      let text_style = TextStyle::Small;
      let row_height = ui.text_style_height(&text_style);

      ui.label(format!("Current Frame: {}", params.currentFrame));

      // ui.horizontal(|ui| {
      //   ui.label("Events: ");
      //   ui.checkbox(&mut opts.showPointerEvents, "Pointer");
      //   ui.add_enabled_ui(opts.showPointerEvents, |ui| {
      //     ui.checkbox(&mut opts.showPointerMoveEvents, "Pointer (move)");
      //   });
      //   ui.checkbox(&mut opts.showTouchEvents, "Touch");
      //   ui.checkbox(&mut opts.showKeyboardEvents, "Keyboard");
      //   if ui.button("Clear").clicked() {
      //     params.clearEvents = true;
      //   }
      //   ui.checkbox(&mut opts.freeze, "Freeze");
      // });

      // ui.allocate_ui(
      //   [ui.available_size().x, ui.available_size().y].into(),
      //   |ui| {
      //     ScrollArea::vertical()
      //       .stick_to_bottom(true)
      //       .auto_shrink([false, false])
      //       .show_rows(ui, row_height, events.len(), |ui, row_range| {
      //         for row in row_range {
      //           ui.horizontal(|ui| {
      //             let event = &events[row];
      //             let consumed_by = native::shards_input_eventConsumedBy(event.evt);
      //             // let c = if native::shards_input_eventIsConsumed(event.evt) {
      //             //   epaint::Color32::WHITE
      //             // } else {
      //             //   epaint::Color32::LIGHT_GRAY
      //             // };

      //             let _type = native::shards_input_eventType(event.evt);
      //             let ecolor = match _type {
      //               1 | 3 => epaint::Color32::LIGHT_BLUE,
      //               5 => epaint::Color32::LIGHT_GREEN,
      //               6 => epaint::Color32::from_rgb(0xb0, 0x5a, 0x24),
      //               _ => epaint::Color32::GRAY,
      //             };
      //             // TODO: Bring back type colors
      //             //   ui.colored_label(epaint::Color32::LIGHT_YELLOW, "[Pointer] ");
      //             //   ui.colored_label(epaint::Color32::LIGHT_RED, "[Keyboard] ");

      //             let cstr = native::shards_input_eventToString(event.evt);
      //             let str = std::ffi::CStr::from_ptr(cstr).to_string_lossy();
      //             ui.colored_label(ecolor, format!("{}: {}", event.frameIndex, str));
      //             native::shards_input_freeString(cstr);

      //             if consumed_by != std::ptr::null_mut() {
      //               let ptr = native::shards_input_layerName(consumed_by);
      //               let name = CStr::from_ptr(ptr).to_str().unwrap();
      //               ui.colored_label(epaint::Color32::WHITE, format!(" [{}]", name));
      //               native::shards_input_freeString(ptr);
      //             }
      //           });
      //           ui.end_row();
      //         }
      //       });
      //   },
      // );

      ui.allocate_rect(ui.available_rect_before_wrap(), Sense::hover())
    });
}
