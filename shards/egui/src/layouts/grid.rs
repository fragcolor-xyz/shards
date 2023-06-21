/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

use super::Grid;
use super::NextRow;
use crate::util;
use crate::EguiId;
use crate::FLOAT2_VAR_SLICE;
use crate::FLOAT_VAR_SLICE;
use crate::HELP_OUTPUT_EQUAL_INPUT;
use crate::HELP_VALUE_IGNORED;
use crate::PARENTS_UI_NAME;
use shards::shard::Shard;
use shards::types::Context;
use shards::types::ExposedTypes;
use shards::types::InstanceData;
use shards::types::OptionalString;
use shards::types::ParamVar;
use shards::types::Parameters;
use shards::types::ShardsVar;
use shards::types::Type;
use shards::types::Types;
use shards::types::Var;
use shards::types::ANY_TYPES;
use shards::types::BOOL_VAR_OR_NONE_SLICE;
use shards::types::SHARDS_OR_NONE_TYPES;

lazy_static! {
  static ref GRID_PARAMETERS: Parameters = vec![
    (
      cstr!("Contents"),
      shccstr!("The UI contents."),
      &SHARDS_OR_NONE_TYPES[..],
    )
      .into(),
    (
      cstr!("Striped"),
      shccstr!("Whether to alternate a subtle background color to every other row."),
      BOOL_VAR_OR_NONE_SLICE,
    )
      .into(),
    (
      cstr!("MinWidth"),
      shccstr!("Minimum column width."),
      FLOAT_VAR_SLICE,
    )
      .into(),
    (
      cstr!("MaxWidth"),
      shccstr!("Maximum column width."),
      FLOAT_VAR_SLICE,
    )
      .into(),
    (
      cstr!("Spacing"),
      shccstr!("Spacing between columns/rows."),
      FLOAT2_VAR_SLICE,
    )
      .into(),
  ];
}

impl Default for Grid {
  fn default() -> Self {
    let mut parents = ParamVar::default();
    parents.set_name(PARENTS_UI_NAME);
    Self {
      parents,
      requiring: Vec::new(),
      contents: ShardsVar::default(),
      striped: ParamVar::new(false.into()),
      min_width: ParamVar::default(),
      max_width: ParamVar::default(),
      spacing: ParamVar::default(),
      exposing: Vec::new(),
    }
  }
}

impl Shard for Grid {
  fn registerName() -> &'static str
  where
    Self: Sized,
  {
    cstr!("UI.Grid")
  }

  fn hash() -> u32
  where
    Self: Sized,
  {
    compile_time_crc32::crc32!("UI.Grid-rust-0x20200101")
  }

  fn name(&mut self) -> &str {
    "UI.Grid"
  }

  fn help(&mut self) -> OptionalString {
    OptionalString(shccstr!("Simple grid layout."))
  }

  fn inputTypes(&mut self) -> &Types {
    &ANY_TYPES
  }

  fn inputHelp(&mut self) -> OptionalString {
    OptionalString(shccstr!(
      "The value that will be passed to the Contents shards of the grid."
    ))
  }

  fn outputTypes(&mut self) -> &Types {
    &ANY_TYPES
  }

  fn outputHelp(&mut self) -> OptionalString {
    *HELP_OUTPUT_EQUAL_INPUT
  }

  fn parameters(&mut self) -> Option<&Parameters> {
    Some(&GRID_PARAMETERS)
  }

  fn setParam(&mut self, index: i32, value: &Var) -> Result<(), &str> {
    match index {
      0 => self.contents.set_param(value),
      1 => Ok(self.striped.set_param(value)),
      2 => Ok(self.min_width.set_param(value)),
      3 => Ok(self.max_width.set_param(value)),
      4 => Ok(self.spacing.set_param(value)),
      _ => Err("Invalid parameter index"),
    }
  }

  fn getParam(&mut self, index: i32) -> Var {
    match index {
      0 => self.contents.get_param(),
      1 => self.striped.get_param(),
      2 => self.min_width.get_param(),
      3 => self.max_width.get_param(),
      4 => self.spacing.get_param(),
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
    self.striped.warmup(ctx);
    self.min_width.warmup(ctx);
    self.max_width.warmup(ctx);
    self.spacing.warmup(ctx);

    Ok(())
  }

  fn cleanup(&mut self) -> Result<(), &str> {
    self.spacing.cleanup();
    self.max_width.cleanup();
    self.min_width.cleanup();
    self.striped.cleanup();
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
      let mut grid = egui::Grid::new(EguiId::new(self, 0));

      let striped = self.striped.get();
      if !striped.is_none() {
        grid = grid.striped(striped.try_into()?);
      }

      let spacing = self.spacing.get();
      if !spacing.is_none() {
        let spacing: (f32, f32) = spacing.try_into()?;
        let spacing: egui::Vec2 = spacing.into();
        grid = grid.spacing(spacing)
      };

      let min_width = self.min_width.get();
      if !min_width.is_none() {
        grid = grid.min_col_width(min_width.try_into()?)
      };

      let max_width = self.max_width.get();
      if !max_width.is_none() {
        grid = grid.max_col_width(max_width.try_into()?)
      };

      grid
        .show(ui, |ui| {
          util::activate_ui_contents(context, input, ui, &mut self.parents, &mut self.contents)
        })
        .inner?;

      // Always passthrough the input
      Ok(*input)
    } else {
      Err("No UI parent")
    }
  }
}

impl Default for NextRow {
  fn default() -> Self {
    let mut parents = ParamVar::default();
    parents.set_name(PARENTS_UI_NAME);
    Self {
      parents,
      requiring: Vec::new(),
    }
  }
}

impl Shard for NextRow {
  fn registerName() -> &'static str
  where
    Self: Sized,
  {
    cstr!("UI.NextRow")
  }

  fn hash() -> u32
  where
    Self: Sized,
  {
    compile_time_crc32::crc32!("UI.NextRow-rust-0x20200101")
  }

  fn name(&mut self) -> &str {
    "UI.NextRow"
  }

  fn help(&mut self) -> OptionalString {
    OptionalString(shccstr!("Moves to the next row in a grid layout."))
  }

  fn inputTypes(&mut self) -> &Types {
    &ANY_TYPES
  }

  fn inputHelp(&mut self) -> OptionalString {
    *HELP_VALUE_IGNORED
  }

  fn outputTypes(&mut self) -> &Types {
    &ANY_TYPES
  }

  fn outputHelp(&mut self) -> OptionalString {
    *HELP_OUTPUT_EQUAL_INPUT
  }

  fn requiredVariables(&mut self) -> Option<&ExposedTypes> {
    self.requiring.clear();

    // Add UI.Parents to the list of required variables
    util::require_parents(&mut self.requiring, &self.parents);

    Some(&self.requiring)
  }

  fn warmup(&mut self, ctx: &Context) -> Result<(), &str> {
    self.parents.warmup(ctx);

    Ok(())
  }

  fn cleanup(&mut self) -> Result<(), &str> {
    self.parents.cleanup();

    Ok(())
  }

  fn activate(&mut self, _context: &Context, input: &Var) -> Result<Var, &str> {
    if let Some(ui) = util::get_current_parent(self.parents.get())? {
      ui.end_row();

      // Always passthrough the input
      Ok(*input)
    } else {
      Err("No UI parent")
    }
  }
}
