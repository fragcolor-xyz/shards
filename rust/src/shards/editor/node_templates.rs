/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2023 Fragcolor Pte. Ltd. */

use super::{graph_ui::UIRenderer, node_factory::NodeTemplateTrait};
use crate::core::{cloneVar, createShard, ShardInstance};
use crate::shardsc::*;
use crate::types::common_type::type2name;
use crate::types::Var;
use egui::{Response, Ui};
use std::ffi::CStr;
use std::ptr::slice_from_raw_parts;

pub(crate) struct NodeTemplate {
  name: &'static str,
  instance: ShardInstance,
}

impl Clone for NodeTemplate {
  fn clone(&self) -> Self {
    // FIXME for now just create a new instance, but we might have to copy some stuff (like parameters' values)
    Self::new(self.name)
  }
}

impl NodeTemplate {
  pub fn new(name: &'static str) -> Self {
    Self {
      name,
      instance: createShard(name),
    }
  }

  pub fn name(&self) -> &'static str {
    self.name
  }
}

// note: trait redirection because enum items are not considered types yet
// FIXME maybe there is a better way, but all this code will eventually disappear once we "generate" it for each shard.

impl NodeTemplateTrait for NodeTemplate {
  fn node_factory_label(&self) -> &str {
    self.name()
  }

  fn node_factory_description(&self) -> &str {
    let str = self.instance.help();
    unsafe {
      if str.string == std::ptr::null_mut() {
        self.node_factory_label()
      } else {
        let slice = CStr::from_ptr(str.string);
        slice.to_str().unwrap_or(self.node_factory_label())
      }
    }
  }
}

impl UIRenderer for NodeTemplate {
  fn ui(&mut self, ui: &mut Ui) -> Response {
    ui.vertical(|ui| {
      let params = self.instance.parameters();
      if params.len > 0 {
        unsafe {
          let params = core::slice::from_raw_parts(params.elements, params.len as usize);
          for p in params {
            ui.label(CStr::from_ptr(p.name).to_str().unwrap());
          }
        }
      }
    })
    .response
  }
}

#[derive(Clone, Default)]
pub(crate) struct VarValue {
  value: Var,
  // FIXME String special case (for now)
  value_s: String,
  // ---
  allowed_types: Vec<SHType>,
  prev_type: SHType,
}

impl VarValue {
  pub fn new(initial_value: &Var, allowed_types: Vec<SHType>) -> Self {
    debug_assert!(allowed_types.len() > 0usize);
    debug_assert!(allowed_types.contains(&initial_value.valueType));

    let mut ret = Self {
      allowed_types,
      ..Default::default()
    };
    // FIXME deal with the String case
    cloneVar(&mut ret.value, initial_value);
    ret
  }
}

fn any_type() -> Vec<SHType> {
  vec![
    // SHType_None, // FIXME need special handling
    // SHType_Any,
    // SHType_Enum, // FIXME add support
    SHType_Bool,
    SHType_Int,
    SHType_Int2,
    SHType_Int3,
    SHType_Int4,
    // SHType_Int8, // FIXME add support
    // SHType_Int16, // FIXME add support
    SHType_Float,
    SHType_Float2,
    SHType_Float3,
    SHType_Float4,
    // SHType_Color, // FIXME add support
    // SHType_ShardRef, // FIXME add support
    // SHType_EndOfBlittableTypes,
    // SHType_Bytes, // FIXME add support
    // SHType_String, // FIXME need special handling
    // SHType_Path, // FIXME add support
    // SHType_ContextVar, // FIXME add support
    // SHType_Image, // FIXME add support
    // SHType_Seq, // FIXME add support
    // SHType_Table, // FIXME add support
    // SHType_Wire, // FIXME add support
    // SHType_Object, // FIXME add support
    // SHType_Array, // FIXME add support
    // SHType_Set, // FIXME add support
    // SHType_Audio, // FIXME add support
  ]
}

impl UIRenderer for VarValue {
  fn ui(&mut self, ui: &mut Ui) -> Response {
    ui.horizontal(|ui| {
      egui::ComboBox::from_id_source(("VarValue", "type"))
        // FIXME display actualy name instead of int
        .selected_text(type2name(self.value.valueType))
        .show_ui(ui, |ui| {
          for t in &self.allowed_types {
            // FIXME display actualy name instead of int
            ui.selectable_value(&mut self.value.valueType, *t, type2name(*t));
          }
        });

      if self.prev_type != self.value.valueType {
        self.prev_type = self.value.valueType;
        // FIXME conversion or reset the value
        // for now just clear the value
        self.value.payload = Default::default();
      }

      unsafe {
        match self.value.valueType {
          SHType_Int => ui.add(egui::DragValue::new(
            &mut self.value.payload.__bindgen_anon_1.intValue,
          )),
          SHType_Int2 => {
            ui.horizontal(|ui| {
              ui.add(egui::DragValue::new(
                &mut self.value.payload.__bindgen_anon_1.int2Value[0],
              ));
              ui.add(egui::DragValue::new(
                &mut self.value.payload.__bindgen_anon_1.int2Value[1],
              ));
            })
            .response
          }
          SHType_Int3 => {
            ui.horizontal(|ui| {
              ui.add(egui::DragValue::new(
                &mut self.value.payload.__bindgen_anon_1.int3Value[0],
              ));
              ui.add(egui::DragValue::new(
                &mut self.value.payload.__bindgen_anon_1.int3Value[1],
              ));
              ui.add(egui::DragValue::new(
                &mut self.value.payload.__bindgen_anon_1.int3Value[2],
              ));
            })
            .response
          }
          SHType_Int4 => {
            ui.horizontal(|ui| {
              ui.add(egui::DragValue::new(
                &mut self.value.payload.__bindgen_anon_1.int4Value[0],
              ));
              ui.add(egui::DragValue::new(
                &mut self.value.payload.__bindgen_anon_1.int4Value[1],
              ));
              ui.add(egui::DragValue::new(
                &mut self.value.payload.__bindgen_anon_1.int4Value[2],
              ));
              ui.add(egui::DragValue::new(
                &mut self.value.payload.__bindgen_anon_1.int4Value[3],
              ));
            })
            .response
          }
          SHType_Float => ui.add(egui::DragValue::new(
            &mut self.value.payload.__bindgen_anon_1.floatValue,
          )),
          SHType_Float2 => {
            ui.horizontal(|ui| {
              ui.add(egui::DragValue::new(
                &mut self.value.payload.__bindgen_anon_1.float2Value[0],
              ));
              ui.add(egui::DragValue::new(
                &mut self.value.payload.__bindgen_anon_1.float2Value[1],
              ));
            })
            .response
          }
          SHType_Float3 => {
            ui.horizontal(|ui| {
              ui.add(egui::DragValue::new(
                &mut self.value.payload.__bindgen_anon_1.float3Value[0],
              ));
              ui.add(egui::DragValue::new(
                &mut self.value.payload.__bindgen_anon_1.float3Value[1],
              ));
              ui.add(egui::DragValue::new(
                &mut self.value.payload.__bindgen_anon_1.float3Value[2],
              ));
            })
            .response
          }
          SHType_Float4 => {
            ui.horizontal(|ui| {
              ui.add(egui::DragValue::new(
                &mut self.value.payload.__bindgen_anon_1.float4Value[0],
              ));
              ui.add(egui::DragValue::new(
                &mut self.value.payload.__bindgen_anon_1.float4Value[1],
              ));
              ui.add(egui::DragValue::new(
                &mut self.value.payload.__bindgen_anon_1.float4Value[2],
              ));
              ui.add(egui::DragValue::new(
                &mut self.value.payload.__bindgen_anon_1.float4Value[3],
              ));
            })
            .response
          }
          SHType_String => {
            // FIXME need special care for string here
            ui.text_edit_singleline(&mut self.value_s)
          }
          _ => ui.colored_label(egui::Color32::RED, "Type of value not supported."),
        }
      }
    })
    .response
  }
}
