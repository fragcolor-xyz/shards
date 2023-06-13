/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2023 Fragcolor Pte. Ltd. */

use crate::widgets::drag_value;
use crate::MutVarTextBuffer;
use crate::UIRenderer;
use egui::*;
use shards::shardsc::*;
use shards::types::Seq;
use shards::types::Table;
use shards::types::Type;
use shards::types::Var;
use std::ffi::CStr;

use super::drag_value::CustomDragValue;

fn get_default_value(value_type: SHType) -> Var {
  match value_type {
    SHType_None => Var::default(),
    SHType_Bool => false.into(),
    SHType_Int => 0.into(),
    SHType_Int2 => (0, 0).into(),
    SHType_Int3 => (0, 0, 0).into(),
    SHType_Int4 => (0, 0, 0, 0).into(),
    SHType_Float => 0.0.into(),
    SHType_Float2 => (0.0, 0.0).into(),
    SHType_Float3 => (0.0, 0.0, 0.0).into(),
    SHType_Float4 => (0.0, 0.0, 0.0, 0.0).into(),
    SHType_String => Var::ephemeral_string(""),
    _ => Var::default(),
  }
}

impl UIRenderer for Var {
  fn render(&mut self, read_only: bool, inner_type: Option<&Type>, ui: &mut Ui) -> Response {
    if read_only && !matches!(self.valueType, SHType_Seq | SHType_Table) {
      ui.set_enabled(false);
    }
    unsafe {
      match self.valueType {
        SHType_None => ui.label(""),
        SHType_Enum => ui.add(CustomDragValue::new(
          &mut self.payload.__bindgen_anon_1.__bindgen_anon_3.enumValue,
        )),
        SHType_Bool => ui.checkbox(&mut self.payload.__bindgen_anon_1.boolValue, ""),
        SHType_Int => ui.add(CustomDragValue::new(
          &mut self.payload.__bindgen_anon_1.intValue,
        )),
        SHType_Int2 => {
          let values = &mut self.payload.__bindgen_anon_1.int2Value;
          let mut ir = ui.horizontal(|ui| {
            ui.add(CustomDragValue::new(&mut values[0])).changed()
              | ui.add(CustomDragValue::new(&mut values[1])).changed()
          });
          ir.response.changed = ir.inner;
          ir.response
        }
        SHType_Int3 => {
          let values = &mut self.payload.__bindgen_anon_1.int3Value;
          let mut ir = ui.horizontal(|ui| {
            ui.add(CustomDragValue::new(&mut values[0])).changed()
              | ui.add(CustomDragValue::new(&mut values[1])).changed()
              | ui.add(CustomDragValue::new(&mut values[2])).changed()
          });
          ir.response.changed = ir.inner;
          ir.response
        }
        SHType_Int4 => {
          let values = &mut self.payload.__bindgen_anon_1.int4Value;
          let mut ir = ui.horizontal(|ui| {
            ui.add(CustomDragValue::new(&mut values[0])).changed()
              | ui.add(CustomDragValue::new(&mut values[1])).changed()
              | ui.add(CustomDragValue::new(&mut values[2])).changed()
              | ui.add(CustomDragValue::new(&mut values[3])).changed()
          });
          ir.response.changed = ir.inner;
          ir.response
        }
        SHType_Int8 => {
          let values = &mut self.payload.__bindgen_anon_1.int8Value;
          let mut ir = ui.vertical(|ui| {
            ui.horizontal(|ui| {
              ui.add(CustomDragValue::new(&mut values[0])).changed()
                | ui.add(CustomDragValue::new(&mut values[1])).changed()
                | ui.add(CustomDragValue::new(&mut values[2])).changed()
                | ui.add(CustomDragValue::new(&mut values[3])).changed()
            })
            .inner
              | ui
                .horizontal(|ui| {
                  ui.add(CustomDragValue::new(&mut values[4])).changed()
                    | ui.add(CustomDragValue::new(&mut values[5])).changed()
                    | ui.add(CustomDragValue::new(&mut values[6])).changed()
                    | ui.add(CustomDragValue::new(&mut values[7])).changed()
                })
                .inner
          });
          ir.response.changed = ir.inner;
          ir.response
        }
        SHType_Int16 => {
          let values = &mut self.payload.__bindgen_anon_1.int16Value;
          let mut ir = ui.vertical(|ui| {
            ui.horizontal(|ui| {
              ui.add(CustomDragValue::new(&mut values[0])).changed()
                | ui.add(CustomDragValue::new(&mut values[1])).changed()
                | ui.add(CustomDragValue::new(&mut values[2])).changed()
                | ui.add(CustomDragValue::new(&mut values[3])).changed()
            })
            .inner
              | ui
                .horizontal(|ui| {
                  ui.add(CustomDragValue::new(&mut values[4])).changed()
                    | ui.add(CustomDragValue::new(&mut values[5])).changed()
                    | ui.add(CustomDragValue::new(&mut values[6])).changed()
                    | ui.add(CustomDragValue::new(&mut values[7])).changed()
                })
                .inner
              | ui
                .horizontal(|ui| {
                  ui.add(CustomDragValue::new(&mut values[8])).changed()
                    | ui.add(CustomDragValue::new(&mut values[9])).changed()
                    | ui.add(CustomDragValue::new(&mut values[10])).changed()
                    | ui.add(CustomDragValue::new(&mut values[11])).changed()
                })
                .inner
              | ui
                .horizontal(|ui| {
                  ui.add(CustomDragValue::new(&mut values[12])).changed()
                    | ui.add(CustomDragValue::new(&mut values[13])).changed()
                    | ui.add(CustomDragValue::new(&mut values[14])).changed()
                    | ui.add(CustomDragValue::new(&mut values[15])).changed()
                })
                .inner
          });
          ir.response.changed = ir.inner;
          ir.response
        }
        SHType_Float => ui.add(CustomDragValue::new(
          &mut self.payload.__bindgen_anon_1.floatValue,
        )),
        SHType_Float2 => {
          let values = &mut self.payload.__bindgen_anon_1.float2Value;
          let mut ir = ui.horizontal(|ui| {
            ui.add(CustomDragValue::new(&mut values[0])).changed()
              | ui.add(CustomDragValue::new(&mut values[1])).changed()
          });
          ir.response.changed = ir.inner;
          ir.response
        }
        SHType_Float3 => {
          let values = &mut self.payload.__bindgen_anon_1.float3Value;
          let mut it = ui.horizontal(|ui| {
            ui.add(CustomDragValue::new(&mut values[0])).changed()
              | ui.add(CustomDragValue::new(&mut values[1])).changed()
              | ui.add(CustomDragValue::new(&mut values[2])).changed()
          });
          it.response.changed = it.inner;
          it.response
        }
        SHType_Float4 => {
          let values = &mut self.payload.__bindgen_anon_1.float4Value;
          let mut it = ui.horizontal(|ui| {
            ui.add(CustomDragValue::new(&mut values[0])).changed()
              | ui.add(CustomDragValue::new(&mut values[1])).changed()
              | ui.add(CustomDragValue::new(&mut values[2])).changed()
              | ui.add(CustomDragValue::new(&mut values[3])).changed()
          });
          it.response.changed = it.inner;
          it.response
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
          let mut buffer = MutVarTextBuffer(self);
          ui.text_edit_singleline(&mut buffer)
        }
        SHType_Seq => {
          let seq = self
            .as_mut_seq()
            .expect("SHType was Seq, but failed to convert to Seq");
          if seq.len() > 0 {
            if let Some(inner_type) = inner_type {
              if !read_only && ui.button("+").clicked() {
                let default_value = get_default_value(inner_type.basicType);
                seq.push(&default_value);
              }
            }
            let mut changed = false;
            let mut ir = ui.collapsing(format!("Seq: {} items", seq.len()), |ui| {
              let mut i = 0usize;
              while i < seq.len() {
                ui.push_id(i, |ui| {
                  changed |= ui
                    .horizontal(|ui| {
                      // for now let's not support sub seqs... we can add that after GDC
                      let response = seq[i].render(read_only, None, ui);
                      if ui.button("-").clicked() {
                        seq.remove(i);
                      }
                      response
                    })
                    .inner
                    .changed();
                });
                i += 1;
              }
            });
            ir.header_response.changed = changed;
            ir.header_response
          } else {
            ui.set_enabled(!read_only);
            if let Some(inner_type) = inner_type {
              if !read_only && ui.button("+").clicked() {
                let default_value = get_default_value(inner_type.basicType);
                seq.push(&default_value);
              }
            }
            ui.colored_label(Color32::YELLOW, "Seq: 0 items")
          }
        }
        SHType_Table => {
          let mut table: Table = self
            .try_into()
            .expect("SHType was Table, but failed to convert to Table");
          if table.len() > 0 {
            let mut changed = false;
            let mut ir = ui.collapsing(format!("Table: {} items", table.len()), |ui| {
              for (mut k, mut _v) in table.iter() {
                ui.push_id(k, |ui| {
                  changed |= ui
                    .horizontal(|ui| {
                      k.render(true, None, ui);
                      // no inner type for now
                      table.get_mut_fast(k).render(read_only, None, ui)
                    })
                    .inner
                    .changed()
                });
              }
            });
            ir.header_response.changed = changed;
            ir.header_response
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
