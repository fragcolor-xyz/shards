/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2023 Fragcolor Pte. Ltd. */

use super::{graph_ui::UIRenderer, node_factory::NodeTemplateTrait};
use crate::shardsc::SHType_Float;
use crate::shardsc::SHType_Float2;
use crate::shardsc::SHType_Float3;
use crate::shardsc::SHType_Float4;
use crate::shardsc::SHType_Int;
use crate::shardsc::SHType_Int2;
use crate::shardsc::SHType_Int3;
use crate::shardsc::SHType_Int4;
use crate::shardsc::SHType_String;
use crate::types::Var;
use egui::{Response, Ui};

#[derive(Clone)]
pub(crate) enum NodeTemplate {
  AssertIs(AssertIsNodeData),
  Const(ConstNodeData),
  ForRange(ForRangeNodeData),
  Get(GetNodeData),
  Log(LogNodeData),
  MathAdd(MathAddNodeData),
  MathDivide(MathDivideNodeData),
  MathMod(MathModNodeData),
  MathMultiply(MathMultiplyNodeData),
  MathSubtract(MathSubtractNodeData),
  Set(SetNodeData),
}

// note: trait redirection because enum items are not considered types yet
// FIXME: maybe there is a better way, but all this code will eventually disappear once we "generate" it for each shard.

impl NodeTemplateTrait for NodeTemplate {
  fn node_factory_description(&self) -> &str {
    match self {
      NodeTemplate::AssertIs(x) => x.node_factory_description(),
      NodeTemplate::Const(x) => x.node_factory_description(),
      NodeTemplate::ForRange(x) => x.node_factory_description(),
      NodeTemplate::Get(x) => x.node_factory_description(),
      NodeTemplate::Log(x) => x.node_factory_description(),
      NodeTemplate::MathAdd(x) => x.node_factory_description(),
      NodeTemplate::MathDivide(x) => x.node_factory_description(),
      NodeTemplate::MathMod(x) => x.node_factory_description(),
      NodeTemplate::MathMultiply(x) => x.node_factory_description(),
      NodeTemplate::MathSubtract(x) => x.node_factory_description(),
      NodeTemplate::Set(x) => x.node_factory_description(),
    }
  }

  fn node_factory_label(&self) -> &str {
    match self {
      NodeTemplate::AssertIs(x) => x.node_factory_label(),
      NodeTemplate::Const(x) => x.node_factory_label(),
      NodeTemplate::ForRange(x) => x.node_factory_label(),
      NodeTemplate::Get(x) => x.node_factory_label(),
      NodeTemplate::Log(x) => x.node_factory_label(),
      NodeTemplate::MathAdd(x) => x.node_factory_label(),
      NodeTemplate::MathDivide(x) => x.node_factory_label(),
      NodeTemplate::MathMod(x) => x.node_factory_label(),
      NodeTemplate::MathMultiply(x) => x.node_factory_label(),
      NodeTemplate::MathSubtract(x) => x.node_factory_label(),
      NodeTemplate::Set(x) => x.node_factory_label(),
    }
  }
}

impl UIRenderer for NodeTemplate {
  fn ui(&mut self, ui: &mut Ui) -> Response {
    match self {
      NodeTemplate::AssertIs(x) => x.ui(ui),
      NodeTemplate::Const(x) => x.ui(ui),
      NodeTemplate::ForRange(x) => x.ui(ui),
      NodeTemplate::Get(x) => x.ui(ui),
      NodeTemplate::Log(x) => x.ui(ui),
      NodeTemplate::MathAdd(MathAddNodeData(x)) => x.ui(ui),
      NodeTemplate::MathDivide(MathDivideNodeData(x)) => x.ui(ui),
      NodeTemplate::MathMod(MathModNodeData(x)) => x.ui(ui),
      NodeTemplate::MathMultiply(MathMultiplyNodeData(x)) => x.ui(ui),
      NodeTemplate::MathSubtract(MathSubtractNodeData(x)) => x.ui(ui),
      NodeTemplate::Set(x) => x.ui(ui),
    }
  }
}

#[derive(Clone, Default)]
pub(crate) struct AssertIsNodeData {
  // FIXME: for now only support 64-bit integer
  pub value: i64,
  pub abort: bool,
}

impl NodeTemplateTrait for AssertIsNodeData {
  fn node_factory_label(&self) -> &str {
    "Assert.Is"
  }

  fn node_factory_description(&self) -> &str {
    "This assertion is used to check whether the input is equal to a given value."
  }
}

impl UIRenderer for AssertIsNodeData {
  fn ui(&mut self, ui: &mut Ui) -> Response {
    ui.vertical(|ui| {
      ui.add(egui::DragValue::new(&mut self.value));
      ui.checkbox(&mut self.abort, "Abort?");
    })
    .response
  }
}

#[derive(Clone, Default)]
pub(crate) struct ConstNodeData {
  pub value_s: String,
  pub value_type: u8,
  pub prev_type: u8,
  pub value: Var,
}

impl NodeTemplateTrait for ConstNodeData {
  fn node_factory_label(&self) -> &str {
    "Const"
  }

  fn node_factory_description(&self) -> &str {
    "Declares an un-named constant value."
  }
}

// FIXME: refactor. This should be part of some util that can be used wherever there is a value.
impl UIRenderer for ConstNodeData {
  fn ui(&mut self, ui: &mut Ui) -> Response {
    ui.horizontal(|ui| {
      egui::ComboBox::from_id_source(("ConstNodeData", "type"))
        // FIXME: display actualy name instead of int
        .selected_text(format!("{:?}", self.value_type))
        .show_ui(ui, |ui| {
          ui.selectable_value(&mut self.value_type, SHType_Int, "Int");
          ui.selectable_value(&mut self.value_type, SHType_Int2, "Int2");
          ui.selectable_value(&mut self.value_type, SHType_Int3, "Int3");
          ui.selectable_value(&mut self.value_type, SHType_Int4, "Int4");
          ui.selectable_value(&mut self.value_type, SHType_Float, "Float");
          ui.selectable_value(&mut self.value_type, SHType_Float2, "Float2");
          ui.selectable_value(&mut self.value_type, SHType_Float3, "Float3");
          ui.selectable_value(&mut self.value_type, SHType_Float4, "Float4");
          ui.selectable_value(&mut self.value_type, SHType_String, "String");
        });

      if self.prev_type != self.value_type {
        self.prev_type = self.value_type;
        // FIXME: conversion or reset the value
        // for now just clear the value
        self.value.payload = Default::default();
        self.value.valueType = self.value_type;
      }

      unsafe {
        match self.value_type {
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
            // FIXME: need special care for string here
            ui.text_edit_singleline(&mut self.value_s)
          }
          _ => ui.colored_label(egui::Color32::RED, "Type of value not supported."),
        }
      }
    })
    .response
  }
}

#[derive(Clone, Default)]
pub(crate) struct ForRangeNodeData {
  // FIXME: for now only support 64-bit integer
  pub from: i64,
  pub to: i64,
}

impl NodeTemplateTrait for ForRangeNodeData {
  fn node_factory_label(&self) -> &str {
    "ForRange"
  }
}

impl UIRenderer for ForRangeNodeData {
  fn ui(&mut self, ui: &mut Ui) -> Response {
    ui.vertical(|ui| {
      ui.add(egui::DragValue::new(&mut self.from));
      if self.from > self.to {
        self.from = self.to;
      }
      ui.add(egui::DragValue::new(&mut self.to));
      if self.to < self.from {
        self.to = self.from;
      }
      // FIXME inner contents
      // contents(ui);
    })
    .response
  }
}

#[derive(Clone, Default)]
pub(crate) struct GetNodeData {
  pub name: String,
}

impl NodeTemplateTrait for GetNodeData {
  fn node_factory_label(&self) -> &str {
    "Get"
  }
}

impl UIRenderer for GetNodeData {
  fn ui(&mut self, ui: &mut Ui) -> Response {
    ui.text_edit_singleline(&mut self.name)
  }
}

#[derive(Clone, Default)]
pub(crate) struct LogNodeData {
  pub label: String,
}

impl NodeTemplateTrait for LogNodeData {
  fn node_factory_label(&self) -> &str {
    "Log"
  }
}

impl UIRenderer for LogNodeData {
  fn ui(&mut self, ui: &mut Ui) -> Response {
    ui.text_edit_singleline(&mut self.label)
  }
}

#[derive(Clone, Default)]
pub(crate) struct MathUnaryNodeData {
  // FIXME: for now only support 64-bit integer
  pub value: i64,
}

#[derive(Clone, Default)]
pub(crate) struct MathAddNodeData(pub MathUnaryNodeData);
#[derive(Clone, Default)]
pub(crate) struct MathDivideNodeData(pub MathUnaryNodeData);
#[derive(Clone, Default)]
pub(crate) struct MathModNodeData(pub MathUnaryNodeData);
#[derive(Clone, Default)]
pub(crate) struct MathMultiplyNodeData(pub MathUnaryNodeData);
#[derive(Clone, Default)]
pub(crate) struct MathSubtractNodeData(pub MathUnaryNodeData);

impl NodeTemplateTrait for MathAddNodeData {
  fn node_factory_label(&self) -> &str {
    "Math.Add"
  }
}

impl NodeTemplateTrait for MathDivideNodeData {
  fn node_factory_label(&self) -> &str {
    "Math.Divide"
  }
}

impl NodeTemplateTrait for MathModNodeData {
  fn node_factory_label(&self) -> &str {
    "Math.Mod"
  }
}

impl NodeTemplateTrait for MathMultiplyNodeData {
  fn node_factory_label(&self) -> &str {
    "Math.Multiply"
  }
}

impl NodeTemplateTrait for MathSubtractNodeData {
  fn node_factory_label(&self) -> &str {
    "Math.Subtract"
  }
}

impl UIRenderer for MathUnaryNodeData {
  fn ui(&mut self, ui: &mut Ui) -> Response {
    ui.add(egui::DragValue::new(&mut self.value))
  }
}

#[derive(Clone, Default)]
pub(crate) struct SetNodeData {
  pub name: String,
}

impl NodeTemplateTrait for SetNodeData {
  fn node_factory_label(&self) -> &str {
    "Set"
  }
}

impl UIRenderer for SetNodeData {
  fn ui(&mut self, ui: &mut Ui) -> Response {
    ui.text_edit_singleline(&mut self.name)
  }
}
