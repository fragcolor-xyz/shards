/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

use super::ProgressBar;
use crate::util;
use crate::FLOAT_VAR_SLICE;
use crate::HELP_OUTPUT_EQUAL_INPUT;
use crate::PARENTS_UI_NAME;
use shards::shard::LegacyShard;
use shards::types::Context;
use shards::types::ExposedTypes;
use shards::types::OptionalString;
use shards::types::ParamVar;
use shards::types::Parameters;
use shards::types::Types;
use shards::types::Var;
use shards::types::FLOAT_TYPES;
use shards::types::STRING_VAR_OR_NONE_SLICE;

lazy_static! {
  static ref PROGRESSBAR_PARAMETERS: Parameters = vec![
    (
      cstr!("Overlay"),
      shccstr!("The text displayed inside the progress bar."),
      STRING_VAR_OR_NONE_SLICE,
    )
      .into(),
    (
      cstr!("Width"),
      shccstr!("The desired width of the progress bar. Will use all horizontal space if not set."),
      FLOAT_VAR_SLICE,
    )
      .into(),
  ];
}

impl Default for ProgressBar {
  fn default() -> Self {
    let mut parents = ParamVar::default();
    parents.set_name(PARENTS_UI_NAME);
    Self {
      parents,
      requiring: Vec::new(),
      overlay: ParamVar::default(),
      desired_width: ParamVar::default(),
    }
  }
}

impl LegacyShard for ProgressBar {
  fn registerName() -> &'static str
  where
    Self: Sized,
  {
    cstr!("UI.ProgressBar")
  }

  fn hash() -> u32
  where
    Self: Sized,
  {
    compile_time_crc32::crc32!("UI.ProgressBar-rust-0x20200101")
  }

  fn name(&mut self) -> &str {
    "UI.ProgressBar"
  }

  fn help(&mut self) -> OptionalString {
    OptionalString(shccstr!("A progress bar with an optional overlay text."))
  }

  fn inputTypes(&mut self) -> &Types {
    &FLOAT_TYPES
  }

  fn inputHelp(&mut self) -> OptionalString {
    OptionalString(shccstr!(
      "The progress amount ranging from 0.0 (no progress) to 1.0 (completed)."
    ))
  }

  fn outputTypes(&mut self) -> &Types {
    &FLOAT_TYPES
  }

  fn outputHelp(&mut self) -> OptionalString {
    *HELP_OUTPUT_EQUAL_INPUT
  }

  fn parameters(&mut self) -> Option<&Parameters> {
    Some(&PROGRESSBAR_PARAMETERS)
  }

  fn setParam(&mut self, index: i32, value: &Var) -> Result<(), &str> {
    match index {
      0 => self.overlay.set_param(value),
      1 => self.desired_width.set_param(value),
      _ => Err("Invalid parameter index"),
    }
  }

  fn getParam(&mut self, index: i32) -> Var {
    match index {
      0 => self.overlay.get_param(),
      1 => self.desired_width.get_param(),
      _ => Var::default(),
    }
  }

  fn requiredVariables(&mut self) -> Option<&ExposedTypes> {
    self.requiring.clear();

    // Add UI.Parents to the list of required variables
    util::require_parents(&mut self.requiring);

    Some(&self.requiring)
  }

  fn warmup(&mut self, ctx: &Context) -> Result<(), &str> {
    self.parents.warmup(ctx);

    self.overlay.warmup(ctx);
    self.desired_width.warmup(ctx);

    Ok(())
  }

  fn cleanup(&mut self, ctx: Option<&Context>) -> Result<(), &str> {
    self.desired_width.cleanup(ctx);
    self.overlay.cleanup(ctx);

    self.parents.cleanup(ctx);

    Ok(())
  }

  fn activate(&mut self, _context: &Context, input: &Var) -> Result<Option<Var>, &str> {
    if let Some(ui) = util::get_current_parent_opt(self.parents.get())? {
      let progress = input.try_into()?;
      let mut progress_bar = egui::ProgressBar::new(progress);

      let overlay = self.overlay.get();
      if !overlay.is_none() {
        let text: &str = overlay.try_into()?;
        progress_bar = progress_bar.text(text);
      }

      let desired_width = self.desired_width.get();
      if !desired_width.is_none() {
        progress_bar = progress_bar.desired_width(desired_width.try_into()?);
      }

      ui.add(progress_bar);

      Ok(None)
    } else {
      Err("No UI parent")
    }
  }
}
