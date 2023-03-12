/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2023 Fragcolor Pte. Ltd. */

use crate::shards::gui::UIRenderer;
use crate::shardsc::*;
use crate::types::Seq;
use crate::types::Table;
use crate::types::Var;
use egui::*;
use std::ffi::CStr;

impl UIRenderer for Var {
  fn render(&mut self, read_only: bool, ui: &mut Ui) -> Response {
    if read_only && !matches!(self.valueType, SHType_Seq | SHType_Table) {
      ui.set_enabled(false);
    }
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
        SHType_String | SHType_Path => {
          let mut mutable = self;
          ui.text_edit_singleline(&mut mutable)
        }
        SHType_Seq => {
          let mut seq: Seq = self
            .try_into()
            .expect("SHType was Seq, but failed to convert to Seq");
          if seq.len() > 0 {
            if !read_only && ui.button("+").clicked() {
              seq.push(&Var::default()); // TODO this is wrong! we need to know the type of the seq
            }
            ui.collapsing(format!("Seq: {} items", seq.len()), |ui| {
              let mut i = 0usize;
              while i < seq.len() {
                ui.push_id(i, |ui| {
                  ui.horizontal(|ui| {
                    seq[i].render(read_only, ui);
                    if ui.button("-").clicked() {
                      seq.remove(i);
                    }
                  });
                });
                i += 1;
              }

              // update self as `seq` len changed
              *self = seq.as_ref().into();
            })
            .header_response
          } else {
            ui.set_enabled(!read_only);
            if !read_only && ui.button("+").clicked() {
              seq.push(&Var::default()); // TODO this is wrong! we need to know the type of the seq

              // update self as `seq` len changed
              *self = seq.as_ref().into();
            }
            ui.colored_label(Color32::YELLOW, "Seq: 0 items")
          }
        }
        SHType_Table => {
          let mut table: Table = self
            .try_into()
            .expect("SHType was Table, but failed to convert to Table");
          if table.len() > 0 {
            ui.collapsing(format!("Table: {} items", table.len()), |ui| {
              for (k, _v) in table.iter() {
                let cstr = CStr::from_ptr(k.0);
                ui.push_id(cstr, |ui| {
                  ui.horizontal(|ui| {
                    ui.label(cstr.to_str().unwrap_or_default());
                    table.get_mut_fast(cstr).render(read_only, ui);
                  });
                });
              }
            })
            .header_response
          } else {
            ui.set_enabled(!read_only);
            ui.colored_label(Color32::YELLOW, "Empty table")
          }
        }
        _ => ui.colored_label(Color32::RED, "Type of variable not supported"),
      }
    }
  }
}
