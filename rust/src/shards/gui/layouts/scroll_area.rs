/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

use super::ScrollArea;
use crate::shard::Shard;
use crate::shards::gui::util;
use crate::shards::gui::EGUI_UI_SEQ_TYPE;
use crate::shards::gui::PARENTS_UI_NAME;
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
use crate::types::ANY_TYPES;
use crate::types::BOOL_TYPES;
use crate::types::SHARDS_OR_NONE_TYPES;

lazy_static! {
  static ref AREA_PARAMETERS: Parameters = vec![
    (
      cstr!("Contents"),
      cstr!("The UI contents."),
      &SHARDS_OR_NONE_TYPES[..],
    )
      .into(),
    (
      cstr!("Horizontal"),
      cstr!("Enable horizontal scrolling."),
      &BOOL_TYPES[..],
    )
      .into(),
    (
      cstr!("Vertical"),
      cstr!("Enable vertical scrolling."),
      &BOOL_TYPES[..],
    )
      .into(),
  ];
}

impl Default for ScrollArea {
  fn default() -> Self {
    let mut parents = ParamVar::default();
    parents.set_name(PARENTS_UI_NAME);
    Self {
      parents,
      requiring: Vec::new(),
      contents: ShardsVar::default(),
      horizontal: ParamVar::new(false.into()),
      vertical: ParamVar::new(true.into()),
      exposing: Vec::new(),
    }
  }
}

impl Shard for ScrollArea {
  fn registerName() -> &'static str
  where
    Self: Sized,
  {
    cstr!("UI.ScrollArea")
  }

  fn hash() -> u32
  where
    Self: Sized,
  {
    compile_time_crc32::crc32!("UI.ScrollArea-rust-0x20200101")
  }

  fn name(&mut self) -> &str {
    "UI.ScrollArea"
  }

  fn inputTypes(&mut self) -> &Types {
    &ANY_TYPES
  }

  fn outputTypes(&mut self) -> &Types {
    &ANY_TYPES
  }

  fn parameters(&mut self) -> Option<&Parameters> {
    Some(&AREA_PARAMETERS)
  }

  fn setParam(&mut self, index: i32, value: &Var) -> Result<(), &str> {
    match index {
      0 => self.contents.set_param(value),
      1 => Ok(self.horizontal.set_param(value)),
      2 => Ok(self.vertical.set_param(value)),
      _ => Err("Invalid parameter index"),
    }
  }

  fn getParam(&mut self, index: i32) -> Var {
    match index {
      0 => self.contents.get_param(),
      1 => self.horizontal.get_param(),
      2 => self.vertical.get_param(),
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

  fn exposedVariables(&mut self) -> Option<&ExposedTypes> {
    self.exposing.clear();

    if !self.contents.is_empty() {
      let exposing = self.contents.get_exposing();
      if let Some(exposing) = exposing {
        for exp in exposing {
          self.exposing.push(*exp);
        }
        Some(&self.exposing)
      } else {
        None
      }
    } else {
      None
    }
  }

  fn hasCompose() -> bool {
    true
  }

  fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
    if !self.contents.is_empty() {
      Ok(self.contents.compose(&data)?)
    } else {
      Ok(data.inputType)
    }
  }

  fn warmup(&mut self, ctx: &Context) -> Result<(), &str> {
    self.parents.warmup(ctx);
    if !self.contents.is_empty() {
      self.contents.warmup(ctx)?;
    }
    self.horizontal.warmup(ctx);
    self.vertical.warmup(ctx);

    Ok(())
  }

  fn cleanup(&mut self) -> Result<(), &str> {
    self.vertical.cleanup();
    self.horizontal.cleanup();
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

    if let Some(ui) = util::get_current_parent(*self.parents.get())? {
      let output = egui::ScrollArea::new([
        self.horizontal.get().try_into()?,
        self.vertical.get().try_into()?,
      ])
      .show(ui, |ui| {
        util::activate_ui_contents(context, input, ui, &mut self.parents, &mut self.contents)
      })
      .inner?;

      Ok(output)
    } else {
      Err("No UI parent")
    }
  }
}
