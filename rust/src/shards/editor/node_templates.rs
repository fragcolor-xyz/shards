/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2023 Fragcolor Pte. Ltd. */

use super::{graph_ui::UIRenderer, node_factory::NodeTemplateTrait};
use egui::Ui;

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
  fn ui(&mut self, ui: &mut Ui) {
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
  fn ui(&mut self, ui: &mut Ui) {
    ui.add(egui::DragValue::new(&mut self.value));
    ui.checkbox(&mut self.abort, "Abort?");
  }
}

#[derive(Clone, Debug, Default, PartialEq)]
pub(crate) enum NodeDataType {
  #[default]
  Int,
  Int2,
  Int3,
  Int4,
  Float,
  Float2,
  Float3,
  Float4,
  String,
}

#[derive(Clone, Default)]
pub(crate) struct ConstNodeData {
  // FIXME: all values should be merged into a single Var, with custom conversion rules
  pub value_i: i64,
  pub value_i2: [i64; 2],
  pub value_i3: [i64; 3],
  pub value_i4: [i64; 4],
  pub value_f: f64,
  pub value_f2: [f64; 2],
  pub value_f3: [f64; 3],
  pub value_f4: [f64; 4],
  pub value_s: String,

  // ...
  pub typ_: NodeDataType,
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
  fn ui(&mut self, ui: &mut Ui) {
    ui.horizontal(|ui| {
      egui::ComboBox::from_id_source(("ConstNodeData", "type"))
        .selected_text(format!("{:?}", self.typ_))
        .show_ui(ui, |ui| {
          ui.selectable_value(&mut self.typ_, NodeDataType::Int, "Int");
          ui.selectable_value(&mut self.typ_, NodeDataType::Int2, "Int2");
          ui.selectable_value(&mut self.typ_, NodeDataType::Int3, "Int3");
          ui.selectable_value(&mut self.typ_, NodeDataType::Int4, "Int4");
          ui.selectable_value(&mut self.typ_, NodeDataType::Float, "Float");
          ui.selectable_value(&mut self.typ_, NodeDataType::Float2, "Float2");
          ui.selectable_value(&mut self.typ_, NodeDataType::Float3, "Float3");
          ui.selectable_value(&mut self.typ_, NodeDataType::Float4, "Float4");
          ui.selectable_value(&mut self.typ_, NodeDataType::String, "String");
        });
      // FIXME: use a single backing store (Var) and apply conversion when switching type (when possible)
      match self.typ_ {
        NodeDataType::Int => {
          ui.add(egui::DragValue::new(&mut self.value_i));
        }
        NodeDataType::Int2 => {
          ui.horizontal(|ui| {
            ui.add(egui::DragValue::new(&mut self.value_i2[0]));
            ui.add(egui::DragValue::new(&mut self.value_i2[1]));
          });
        }
        NodeDataType::Int3 => {
          ui.horizontal(|ui| {
            ui.add(egui::DragValue::new(&mut self.value_i3[0]));
            ui.add(egui::DragValue::new(&mut self.value_i3[1]));
            ui.add(egui::DragValue::new(&mut self.value_i3[2]));
          });
        }
        NodeDataType::Int4 => {
          ui.horizontal(|ui| {
            ui.add(egui::DragValue::new(&mut self.value_i4[0]));
            ui.add(egui::DragValue::new(&mut self.value_i4[1]));
            ui.add(egui::DragValue::new(&mut self.value_i4[2]));
            ui.add(egui::DragValue::new(&mut self.value_i4[3]));
          });
        }
        NodeDataType::Float => {
          ui.add(egui::DragValue::new(&mut self.value_f));
        }
        NodeDataType::Float2 => {
          ui.horizontal(|ui| {
            ui.add(egui::DragValue::new(&mut self.value_f2[0]));
            ui.add(egui::DragValue::new(&mut self.value_f2[1]));
          });
        }
        NodeDataType::Float3 => {
          ui.horizontal(|ui| {
            ui.add(egui::DragValue::new(&mut self.value_f3[0]));
            ui.add(egui::DragValue::new(&mut self.value_f3[1]));
            ui.add(egui::DragValue::new(&mut self.value_f3[2]));
          });
        }
        NodeDataType::Float4 => {
          ui.horizontal(|ui| {
            ui.add(egui::DragValue::new(&mut self.value_f4[0]));
            ui.add(egui::DragValue::new(&mut self.value_f4[1]));
            ui.add(egui::DragValue::new(&mut self.value_f4[2]));
            ui.add(egui::DragValue::new(&mut self.value_f4[3]));
          });
        }
        NodeDataType::String => {
          ui.text_edit_singleline(&mut self.value_s);
        }
        _ => {}
      }
    });
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
  fn ui(&mut self, ui: &mut Ui) {
    ui.add(egui::DragValue::new(&mut self.from));
    if self.from > self.to {
      self.from = self.to;
    }
    ui.add(egui::DragValue::new(&mut self.to));
    if self.to < self.from {
      self.to = self.from;
    }
    // FIXME: inner contents
    // contents(ui);
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
  fn ui(&mut self, ui: &mut Ui) {
    ui.text_edit_singleline(&mut self.name);
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
  fn ui(&mut self, ui: &mut Ui) {
    ui.text_edit_singleline(&mut self.label);
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
  fn ui(&mut self, ui: &mut Ui) {
    ui.add(egui::DragValue::new(&mut self.value));
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
  fn ui(&mut self, ui: &mut Ui) {
    ui.text_edit_singleline(&mut self.name);
  }
}
