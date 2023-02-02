/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2023 Fragcolor Pte. Ltd. */

use super::{graph_ui::UIRenderer, node_factory::NodeTemplateTrait};
use crate::core::{cloneVar, createShard, ShardInstance};
use crate::shardsc::*;
use crate::types::common_type::type2name;
use crate::types::Var;
use egui::{Response, Ui};
use std::collections::BTreeMap;
use std::ffi::CStr;
use std::ptr::slice_from_raw_parts;

#[derive(Clone)]
pub(crate) struct ShardTemplate<'a> {
  name: &'a str,
  help: &'a str,
}

impl<'a> ShardTemplate<'a> {
  pub fn new(name: &'a str) -> Self {
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

impl<'a> NodeTemplateTrait for ShardTemplate<'a> {
  type NodeData = ShardData<'a>;

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

impl<'a> UIRenderer for ShardTemplate<'a> {
  fn ui(&mut self, ui: &mut Ui) -> Response {
    ui.label(self.help)
  }
}

pub(crate) struct ShardData<'a> {
  name: &'a str,
  instance: ShardInstance,
  params: Vec<(&'a str, VarValue<'a>)>,
}

impl<'a> ShardData<'a> {
  fn new(name: &'a str) -> Self {
    let instance = createShard(name);
    instance.into()
  }
}

impl<'a> From<ShardInstance> for ShardData<'a> {
  fn from(instance: ShardInstance) -> Self {
    unsafe {
      let name = CStr::from_ptr(instance.name()).to_str().unwrap();
      let params = instance.parameters();
      let params = if params.len > 0 {
        let params = core::slice::from_raw_parts(params.elements, params.len as usize);
        params
          .iter()
          .enumerate()
          .map(|(i, p)| {
            let mut data = VarValueData::Basic;
            let types = p.valueTypes;
            let mut types = if types.len > 0 {
              let types = core::slice::from_raw_parts(types.elements, types.len as usize);
              types
                .iter()
                .map(|t| {
                  if t.basicType == SHType_Enum {
                    let details = t.details.enumeration;
                    let info = util_findEnumInfo(details.vendorId, details.typeId)
                      .as_ref()
                      .unwrap();
                    let name = CStr::from_ptr(info.name).to_str().unwrap();
                    let labels =
                      core::slice::from_raw_parts(info.labels.elements, info.labels.len as usize);
                    let labels: Vec<&str> = labels
                      .iter()
                      .map(|l| CStr::from_ptr(*l).to_str().unwrap())
                      .collect();
                    let descriptions = core::slice::from_raw_parts(
                      info.descriptions.elements,
                      info.descriptions.len as usize,
                    );
                    let descriptions: Vec<&str> = descriptions
                      .iter()
                      .map(|l| {
                        if l.string == std::ptr::null_mut() {
                          ""
                        } else {
                          CStr::from_ptr(l.string).to_str().unwrap()
                        }
                      })
                      .collect();
                    let values =
                      core::slice::from_raw_parts(info.values.elements, info.values.len as usize);

                    assert_eq!(values.len(), labels.len());
                    assert_eq!(labels.len(), descriptions.len());

                    // FIXME have a cache for enums
                    // FIXME support multiple enum types
                    data = VarValueData::Enum {
                      enum_name: name,
                      enum_values: (0..labels.len())
                        .map(|i| (values[i], (labels[i], descriptions[i])))
                        .collect(),
                    };
                  }
                  t.basicType
                })
                .collect()
            } else {
              vec![]
            };
            // TODO deal with ContextVar
            // deal with Any
            if types.contains(&SHType_Any) {
              // FIXME might also need Any's ContextVar
              // FIXME might also need to restore some Enum values
              //       note: we expect them to be explicitly allowed in the parameter info as we
              //       can offer all enum types here (except maybe for radio button)
              types = any_type();
            }

            let type_name = CStr::from_ptr(p.name).to_str().unwrap();
            let initial_value = instance.getParam(i as i32);
            (type_name, VarValue::new(&initial_value, types, data))
          })
          .collect()
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
}

impl<'a> UIRenderer for ShardData<'a> {
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

pub(crate) struct VarValue<'a> {
  value: Var,
  allowed_types: Vec<SHType>,
  prev_type: SHType,
  // FIXME use this to "restore" previous value
  // prev_value: Option<SHVarPayload>,
  data: VarValueData<'a>,
}

pub(crate) enum VarValueData<'a> {
  Basic,
  Enum {
    enum_name: &'a str,
    enum_values: BTreeMap<i32, (&'a str, &'a str)>,
  },
}

impl<'a> Clone for VarValue<'a> {
  fn clone(&self) -> Self {
    VarValue::new(&self.value, self.allowed_types.clone(), self.data.clone())
  }
}

impl<'a> Clone for VarValueData<'a> {
  fn clone(&self) -> Self {
    match self {
      Self::Basic => Self::Basic,
      Self::Enum {
        enum_name,
        enum_values,
      } => Self::Enum {
        enum_name,
        enum_values: enum_values.clone(),
      },
    }
  }
}

impl<'a> VarValue<'a> {
  pub fn new(initial_value: &Var, allowed_types: Vec<SHType>, data: VarValueData<'a>) -> Self {
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
      value: Var::default(),
      allowed_types,
      prev_type: initial_value.valueType,
      data,
    };
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
    SHType_Color,
    // SHType_ShardRef, // FIXME add support
    // SHType_EndOfBlittableTypes,
    // SHType_Bytes, // FIXME add support
    SHType_String,
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

macro_rules! invalid_data {
  ($ui:ident) => {
    if cfg!(debug_assertions) {
      $ui.colored_label(egui::Color32::RED, "Invalid type of VarValueData")
    } else {
      $ui.label("")
    }
  };
}

impl<'a> UIRenderer for VarValue<'a> {
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
          SHType_Enum => {
            if let VarValueData::Enum {
              enum_name,
              enum_values,
            } = &mut self.data
            {
              let value = &mut self
                .value
                .payload
                .__bindgen_anon_1
                .__bindgen_anon_3
                .enumValue;
              egui::ComboBox::from_label(*enum_name)
                .selected_text(enum_values.get(value).unwrap_or(&("", "")).0)
                .show_ui(ui, |ui| {
                  for i in enum_values.keys() {
                    ui.selectable_value(value, *i, enum_values.get(i).unwrap().0);
                  }
                })
                .response
            } else {
              invalid_data!(ui)
            }
          }
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
          SHType_Color => {
            let color = &mut self.value.payload.__bindgen_anon_1.colorValue;
            let mut srgba = [color.r, color.g, color.b, color.a];
            let response = ui.color_edit_button_srgba_unmultiplied(&mut srgba);
            color.r = srgba[0];
            color.g = srgba[1];
            color.b = srgba[2];
            color.a = srgba[3];
            response
          }
          SHType_String => {
            let mut mutable = &mut self.value;
            ui.text_edit_singleline(&mut mutable)
          }
          _ => ui.colored_label(egui::Color32::RED, "Type of value not supported."),
        }
      }
    })
    .response
  }
}
