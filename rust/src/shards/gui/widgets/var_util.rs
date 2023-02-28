/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2023 Fragcolor Pte. Ltd. */

use crate::shards::gui::UIRenderer;
use crate::shardsc::*;
use crate::types::Seq;
use crate::types::Var;
use egui::*;

impl UIRenderer for Var {
  fn render(&mut self, ui: &mut Ui) -> Response {
    unsafe {
      match self.valueType {
        SHType_None => ui.label(""),
        SHType_Enum => ui.add(DragValue::new(
          &mut self.payload.__bindgen_anon_1.__bindgen_anon_3.enumValue,
        )),
        SHType_Bool => ui.checkbox(&mut self.payload.__bindgen_anon_1.boolValue, ""),
        SHType_Int => ui.add(DragValue::new(&mut self.payload.__bindgen_anon_1.intValue)),
        SHType_Int2 => {
          let values = &mut self.payload.__bindgen_anon_1.int2Value;
          ui.horizontal(|ui| {
            ui.add(DragValue::new(&mut values[0]));
            ui.add(DragValue::new(&mut values[1]));
          })
          .response
        }
        SHType_Int3 => {
          let values = &mut self.payload.__bindgen_anon_1.int3Value;
          ui.horizontal(|ui| {
            ui.add(DragValue::new(&mut values[0]));
            ui.add(DragValue::new(&mut values[1]));
            ui.add(DragValue::new(&mut values[2]));
          })
          .response
        }
        SHType_Int4 => {
          let values = &mut self.payload.__bindgen_anon_1.int4Value;
          ui.horizontal(|ui| {
            ui.add(DragValue::new(&mut values[0]));
            ui.add(DragValue::new(&mut values[1]));
            ui.add(DragValue::new(&mut values[2]));
            ui.add(DragValue::new(&mut values[3]));
          })
          .response
        }
        SHType_Int8 => {
          let values = &mut self.payload.__bindgen_anon_1.int8Value;
          ui.vertical(|ui| {
            ui.horizontal(|ui| {
              ui.add(DragValue::new(&mut values[0]));
              ui.add(DragValue::new(&mut values[1]));
              ui.add(DragValue::new(&mut values[2]));
              ui.add(DragValue::new(&mut values[3]));
            });
            ui.horizontal(|ui| {
              ui.add(DragValue::new(&mut values[4]));
              ui.add(DragValue::new(&mut values[5]));
              ui.add(DragValue::new(&mut values[6]));
              ui.add(DragValue::new(&mut values[7]));
            });
          })
          .response
        }
        SHType_Int16 => {
          let values = &mut self.payload.__bindgen_anon_1.int16Value;
          ui.vertical(|ui| {
            ui.horizontal(|ui| {
              ui.add(DragValue::new(&mut values[0]));
              ui.add(DragValue::new(&mut values[1]));
              ui.add(DragValue::new(&mut values[2]));
              ui.add(DragValue::new(&mut values[3]));
            });
            ui.horizontal(|ui| {
              ui.add(DragValue::new(&mut values[4]));
              ui.add(DragValue::new(&mut values[5]));
              ui.add(DragValue::new(&mut values[6]));
              ui.add(DragValue::new(&mut values[7]));
            });
            ui.horizontal(|ui| {
              ui.add(DragValue::new(&mut values[8]));
              ui.add(DragValue::new(&mut values[9]));
              ui.add(DragValue::new(&mut values[10]));
              ui.add(DragValue::new(&mut values[11]));
            });
            ui.horizontal(|ui| {
              ui.add(DragValue::new(&mut values[12]));
              ui.add(DragValue::new(&mut values[13]));
              ui.add(DragValue::new(&mut values[14]));
              ui.add(DragValue::new(&mut values[15]));
            });
          })
          .response
        }
        SHType_Float => ui.add(DragValue::new(
          &mut self.payload.__bindgen_anon_1.floatValue,
        )),
        SHType_Float2 => {
          let values = &mut self.payload.__bindgen_anon_1.float2Value;
          ui.horizontal(|ui| {
            ui.add(DragValue::new(&mut values[0]));
            ui.add(DragValue::new(&mut values[1]));
          })
          .response
        }
        SHType_Float3 => {
          let values = &mut self.payload.__bindgen_anon_1.float3Value;
          ui.horizontal(|ui| {
            ui.add(DragValue::new(&mut values[0]));
            ui.add(DragValue::new(&mut values[1]));
            ui.add(DragValue::new(&mut values[2]));
          })
          .response
        }
        SHType_Float4 => {
          let values = &mut self.payload.__bindgen_anon_1.float4Value;
          ui.horizontal(|ui| {
            ui.add(DragValue::new(&mut values[0]));
            ui.add(DragValue::new(&mut values[1]));
            ui.add(DragValue::new(&mut values[2]));
            ui.add(DragValue::new(&mut values[3]));
          })
          .response
        }
        SHType_Color => {
          let color = &mut self.payload.__bindgen_anon_1.colorValue;
          let mut srgba = [color.r, color.g, color.b, color.a];
          let response = ui.color_edit_button_srgba_unmultiplied(&mut srgba);
          color.r = srgba[0];
          color.g = srgba[1];
          color.b = srgba[2];
          color.a = srgba[3];
          response
        }
        SHType_String => {
          let mut mutable = self;
          ui.text_edit_singleline(&mut mutable)
        }
        SHType_Seq => {
          let mut seq: Seq = self.try_into().unwrap_or_default();
          if seq.len() > 0 {
            ui.collapsing(format!("Seq: {} items", seq.len()), |ui| {
              for i in 0..seq.len() {
                ui.push_id(i, |ui| {
                  seq[i].render(ui);
                });
              }
            })
            .header_response
          } else {
            ui.colored_label(Color32::YELLOW, "Empty sequence")
          }
        }
        _ => ui.colored_label(Color32::RED, "Type of variable not supported"),
      }
    }
  }
}
