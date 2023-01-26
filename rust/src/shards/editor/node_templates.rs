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

#[derive(Clone)]
pub(crate) struct ShardTemplate {
  name: &'static str,
  help: &'static str,
}

impl ShardTemplate {
  pub fn new(name: &'static str) -> Self {
    let instance = createShard(name);
    let help = instance.help();
    let help = unsafe {
      if help.string == std::ptr::null_mut() {
        ""
      } else {
        let slice = CStr::from_ptr(help.string);
        slice.to_str().unwrap_or("")
      }
    };

    Self { name, help }
  }
}

impl NodeTemplateTrait for ShardTemplate {
  type NodeData = ShardData;

  fn node_factory_description(&self) -> &str {
    self.help
  }

  fn node_factory_label(&self) -> &str {
    self.name
  }

  fn build_node(&self) -> Self::NodeData {
    ShardData::new(self.name)
  }
}

impl UIRenderer for ShardTemplate {
  fn ui(&mut self, ui: &mut Ui) -> Response {
    ui.label(self.help)
  }
}

pub(crate) struct ShardData {
  name: &'static str,
  instance: ShardInstance,
  params: Vec<(&'static str, VarValue)>,
}

impl ShardData {
  fn new(name: &'static str) -> Self {
    let instance = createShard(name);
    let params = instance.parameters();
    let params = if params.len > 0 {
      unsafe {
        let params = core::slice::from_raw_parts(params.elements, params.len as usize);
        params
          .iter()
          .enumerate()
          .map(|(i, p)| {
            let types = p.valueTypes;
            let types = if types.len > 0 {
              let types = core::slice::from_raw_parts(types.elements, types.len as usize);
              types.iter().map(|t| t.basicType).collect()
            } else {
              vec![]
            };
            // FIXME: deal with Any

            let name = CStr::from_ptr(p.name).to_str().unwrap();
            let initial_value = instance.getParam(i as i32);
            (name, VarValue::new(&initial_value, types))
          })
          .collect()
      }
    } else {
      vec![]
    };

    Self {
      name,
      instance,
      params,
    }
  }
}

impl UIRenderer for ShardData {
  fn ui(&mut self, ui: &mut Ui) -> Response {
    egui::Grid::new(self.name)
      .show(ui, |ui| {
        for (i, (name, p)) in self.params.iter_mut().enumerate() {
          ui.label(*name);
          ui.push_id(i, |ui| p.ui(ui));
          ui.end_row();
        }
      })
      .response
  }
}

#[derive(Default)]
pub(crate) struct VarValue {
  value: Var,
  // FIXME String special case (for now)
  value_s: String,
  // ---
  allowed_types: Vec<SHType>,
  prev_type: SHType,
  // FIXME use this to "restore" previous value
  // prev_value: Option<SHVarPayload>,
}

impl Clone for VarValue {
  fn clone(&self) -> Self {
    VarValue::new(&self.value, self.allowed_types.clone())
  }
}

impl VarValue {
  pub fn new(initial_value: &Var, allowed_types: Vec<SHType>) -> Self {
    debug_assert!(allowed_types.len() > 0usize);
    if cfg!(debug_assertions) {
      if !allowed_types.contains(&initial_value.valueType) {
        // HACK: put a breakpoint here or uncomment
        shlog_debug!(
          "Type {} is not amongst allowed types {:?}",
          initial_value.valueType,
          allowed_types
        );
      }
    }

    let mut ret = Self {
      allowed_types,
      ..Default::default()
    };
    // FIXME deal with the String case
    cloneVar(&mut ret.value, initial_value);
    ret.prev_type = ret.value.valueType;
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
          SHType_None => ui.label(""),
          SHType_Bool => ui.checkbox(&mut self.value.payload.__bindgen_anon_1.boolValue, ""),
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
