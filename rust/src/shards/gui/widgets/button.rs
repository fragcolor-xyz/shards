/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

use super::Button;
use crate::shard::Shard;
use crate::shards::gui::util;
use crate::shards::gui::BOOL_OR_NONE_SLICE;
use crate::shards::gui::EGUI_UI_SEQ_TYPE;
use crate::shards::gui::PARENTS_UI_NAME;
use crate::types::common_type;
use crate::types::Context;
use crate::types::ExposedInfo;
use crate::types::ExposedTypes;
use crate::types::InstanceData;
use crate::types::ParamVar;
use crate::types::Parameters;
use crate::types::ShardsVar;
use crate::types::Type;
use crate::types::Types;
use crate::types::Var;
use crate::types::WireState;
use crate::types::BOOL_TYPES;
use crate::types::NONE_TYPES;
use crate::types::SHARDS_OR_NONE_TYPES;
use crate::types::STRING_TYPES;

lazy_static! {
  static ref BUTTON_PARAMETERS: Parameters = vec![
    (
      cstr!("Label"),
      cstr!("The text label of this button."),
      &STRING_TYPES[..],
    )
      .into(),
    (
      cstr!("Action"),
      cstr!("The shards to execute when the button is pressed."),
      &SHARDS_OR_NONE_TYPES[..],
    )
      .into(),
    (
      cstr!("Wrap"),
      cstr!("Wrap the text depending on the layout."),
      BOOL_OR_NONE_SLICE,
    )
      .into(),
  ];
}

impl Default for Button {
  fn default() -> Self {
    let mut parents = ParamVar::default();
    parents.set_name(PARENTS_UI_NAME);
    Self {
      parents,
      requiring: Vec::new(),
      label: ParamVar::default(),
      action: ShardsVar::default(),
      wrap: ParamVar::default(),
    }
  }
}

impl Shard for Button {
  fn registerName() -> &'static str
  where
    Self: Sized,
  {
    cstr!("UI.Button")
  }

  fn hash() -> u32
  where
    Self: Sized,
  {
    compile_time_crc32::crc32!("UI.Button-rust-0x20200101")
  }

  fn name(&mut self) -> &str {
    "UI.Button"
  }

  fn inputTypes(&mut self) -> &Types {
    &NONE_TYPES
  }

  fn outputTypes(&mut self) -> &Types {
    &BOOL_TYPES
  }

  fn parameters(&mut self) -> Option<&Parameters> {
    Some(&BUTTON_PARAMETERS)
  }

  fn setParam(&mut self, index: i32, value: &Var) -> Result<(), &str> {
    match index {
      0 => Ok(self.label.set_param(value)),
      1 => self.action.set_param(value),
      2 => Ok(self.wrap.set_param(value)),
      _ => Err("Invalid parameter index"),
    }
  }

  fn getParam(&mut self, index: i32) -> Var {
    match index {
      0 => self.label.get_param(),
      1 => self.action.get_param(),
      2 => self.wrap.get_param(),
      _ => Var::default(),
    }
  }

  fn requiredVariables(&mut self) -> Option<&ExposedTypes> {
    self.requiring.clear();

    // Add UI.Parents to the list of required variables
    let exp_info = ExposedInfo {
      exposedType: EGUI_UI_SEQ_TYPE,
      name: self.parents.get_name(),
      help: cstr!("The parent UI objects.").into(),
      ..ExposedInfo::default()
    };
    self.requiring.push(exp_info);

    Some(&self.requiring)
  }

  fn hasCompose() -> bool {
    true
  }

  fn compose(&mut self, data: &mut InstanceData) -> Result<Type, &str> {
    if !self.action.is_empty() {
      self.action.compose(&mut data)?;
    }

    Ok(common_type::bool)
  }

  fn warmup(&mut self, ctx: &Context) -> Result<(), &str> {
    self.parents.warmup(ctx);

    self.label.warmup(ctx);
    if !self.action.is_empty() {
      self.action.warmup(ctx)?;
    }
    self.wrap.warmup(ctx);

    Ok(())
  }

  fn cleanup(&mut self) -> Result<(), &str> {
    self.wrap.cleanup();
    if !self.action.is_empty() {
      self.action.cleanup();
    }
    self.label.cleanup();

    self.parents.cleanup();

    Ok(())
  }

  fn activate(&mut self, context: &Context, input: &Var) -> Result<Var, &str> {
    if let Some(ui) = util::get_current_parent(self.parents.get())? {
      let label: &str = self.label.get().as_ref().try_into()?;
      let mut button = egui::Button::new(label);

      let wrap = self.wrap.get();
      if !wrap.is_none() {
        let wrap: bool = wrap.as_ref().try_into()?;
        button = button.wrap(wrap);
      }

      let response = ui.add(button);
      if response.clicked() {
        let mut output = Var::default();
        if self.action.activate(context, input, &mut output) == WireState::Error {
          return Err("Failed to activate button");
        }

        // button clicked during this frame
        return Ok(true.into());
      }

      // button not clicked during this frame
      Ok(false.into())
    } else {
      Err("No UI parent")
    }
  }
}
