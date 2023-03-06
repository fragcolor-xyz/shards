/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

use super::Vertical;
use crate::shard::Shard;
use crate::shards::gui::util;
use crate::shards::gui::HELP_OUTPUT_EQUAL_INPUT;
use crate::shards::gui::PARENTS_UI_NAME;
use crate::types::Context;
use crate::types::ExposedTypes;
use crate::types::InstanceData;
use crate::types::OptionalString;
use crate::types::ParamVar;
use crate::types::Parameters;
use crate::types::ShardsVar;
use crate::types::Type;
use crate::types::Types;
use crate::types::Var;
use crate::types::ANY_TYPES;
use crate::types::BOOL_VAR_OR_NONE_SLICE;
use crate::types::SHARDS_OR_NONE_TYPES;

lazy_static! {
  static ref VERTICAL_PARAMETERS: Parameters = vec![
    (
      cstr!("Contents"),
      shccstr!("The UI contents."),
      &SHARDS_OR_NONE_TYPES[..],
    )
      .into(),
    (
      cstr!("Centered"),
      shccstr!("Center the contents horizontally."),
      BOOL_VAR_OR_NONE_SLICE,
    )
      .into(),
  ];
}

impl Default for Vertical {
  fn default() -> Self {
    let mut parents = ParamVar::default();
    parents.set_name(PARENTS_UI_NAME);
    Self {
      parents,
      requiring: Vec::new(),
      contents: ShardsVar::default(),
      centered: ParamVar::default(),
      exposing: Vec::new(),
    }
  }
}

impl Shard for Vertical {
  fn registerName() -> &'static str
  where
    Self: Sized,
  {
    cstr!("UI.Vertical")
  }

  fn hash() -> u32
  where
    Self: Sized,
  {
    compile_time_crc32::crc32!("UI.Vertical-rust-0x20200101")
  }

  fn name(&mut self) -> &str {
    "UI.Vertical"
  }

  fn help(&mut self) -> OptionalString {
    OptionalString(shccstr!("Layout the contents vertically."))
  }

  fn inputTypes(&mut self) -> &Types {
    &ANY_TYPES
  }

  fn inputHelp(&mut self) -> OptionalString {
    OptionalString(shccstr!(
      "The value that will be passed to the Contents shards of the layout."
    ))
  }

  fn outputTypes(&mut self) -> &Types {
    &ANY_TYPES
  }

  fn outputHelp(&mut self) -> OptionalString {
    *HELP_OUTPUT_EQUAL_INPUT
  }

  fn parameters(&mut self) -> Option<&Parameters> {
    Some(&VERTICAL_PARAMETERS)
  }

  fn setParam(&mut self, index: i32, value: &Var) -> Result<(), &str> {
    match index {
      0 => self.contents.set_param(value),
      1 => Ok(self.centered.set_param(value)),
      _ => Err("Invalid parameter index"),
    }
  }

  fn getParam(&mut self, index: i32) -> Var {
    match index {
      0 => self.contents.get_param(),
      1 => self.centered.get_param(),
      _ => Var::default(),
    }
  }

  fn requiredVariables(&mut self) -> Option<&ExposedTypes> {
    self.requiring.clear();

    // Add UI.Parents to the list of required variables
    util::require_parents(&mut self.requiring, &self.parents);

    Some(&self.requiring)
  }

  fn exposedVariables(&mut self) -> Option<&ExposedTypes> {
    self.exposing.clear();

    if util::expose_contents_variables(&mut self.exposing, &self.contents) {
      Some(&self.exposing)
    } else {
      None
    }
  }

  fn hasCompose() -> bool {
    true
  }

  fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
    if !self.contents.is_empty() {
      self.contents.compose(data)?;
    }

    // Always passthrough the input
    Ok(data.inputType)
  }

  fn warmup(&mut self, ctx: &Context) -> Result<(), &str> {
    self.parents.warmup(ctx);
    if !self.contents.is_empty() {
      self.contents.warmup(ctx)?;
    }
    self.centered.warmup(ctx);

    Ok(())
  }

  fn cleanup(&mut self) -> Result<(), &str> {
    self.centered.cleanup();
    if !self.contents.is_empty() {
      self.contents.cleanup();
    }
    self.parents.cleanup();

    Ok(())
  }

  fn activate(&mut self, context: &Context, input: &Var) -> Result<Var, &str> {
    if self.contents.is_empty() {
      return Ok(*input);
    }

    if let Some(ui) = util::get_current_parent(self.parents.get())? {
      if self.centered.get().try_into().unwrap_or_default() {
        ui.vertical_centered(|ui| {
          util::activate_ui_contents(context, input, ui, &mut self.parents, &mut self.contents)
        })
        .inner?
      } else {
        ui.vertical(|ui| {
          util::activate_ui_contents(context, input, ui, &mut self.parents, &mut self.contents)
        })
        .inner?
      };

      // Always passthrough the input
      Ok(*input)
    } else {
      Err("No UI parent")
    }
  }
}
