/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

use super::ProgressBar;
use crate::shard::Shard;
use crate::shards::gui::util;
use crate::shards::gui::EGUI_UI_SEQ_TYPE;
use crate::shards::gui::PARENTS_UI_NAME;
use crate::types::Context;
use crate::types::ExposedInfo;
use crate::types::ExposedTypes;
use crate::types::OptionalString;
use crate::types::ParamVar;
use crate::types::Parameters;
use crate::types::Types;
use crate::types::Var;
use crate::types::FLOAT_TYPES;
use crate::types::STRING_OR_NONE_SLICE;

lazy_static! {
  static ref PROGRESSBAR_PARAMETERS: Parameters = vec![(
    cstr!("Overlay"),
    cstr!("The text displayed inside the progress bar."),
    STRING_OR_NONE_SLICE,
  )
    .into(),];
}

impl Default for ProgressBar {
  fn default() -> Self {
    let mut parents = ParamVar::default();
    parents.set_name(PARENTS_UI_NAME);
    Self {
      parents,
      requiring: Vec::new(),
      overlay: ParamVar::default(),
    }
  }
}

impl Shard for ProgressBar {
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
      "The progress amount in the [0.0, 1.0] range, where 1 means completed."
    ))
  }

  fn outputTypes(&mut self) -> &Types {
    &FLOAT_TYPES
  }

  fn outputHelp(&mut self) -> OptionalString {
    OptionalString(shccstr!("The output of this shard will be its input."))
  }

  fn parameters(&mut self) -> Option<&Parameters> {
    Some(&PROGRESSBAR_PARAMETERS)
  }

  fn setParam(&mut self, index: i32, value: &Var) -> Result<(), &str> {
    match index {
      0 => Ok(self.overlay.set_param(value)),
      _ => Err("Invalid parameter index"),
    }
  }

  fn getParam(&mut self, index: i32) -> Var {
    match index {
      0 => self.overlay.get_param(),
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

  fn warmup(&mut self, ctx: &Context) -> Result<(), &str> {
    self.parents.warmup(ctx);

    self.overlay.warmup(ctx);

    Ok(())
  }

  fn cleanup(&mut self) -> Result<(), &str> {
    self.overlay.cleanup();

    self.parents.cleanup();

    Ok(())
  }

  fn activate(&mut self, _context: &Context, input: &Var) -> Result<Var, &str> {
    if let Some(ui) = util::get_current_parent(*self.parents.get())? {
      let progress = input.try_into()?;
      let mut progressBar = egui::ProgressBar::new(progress);

      if !self.overlay.get().is_none() {
        let text: &str = self.overlay.get().try_into()?;
        progressBar = progressBar.text(text);
      }

      ui.add(progressBar);

      Ok(*input)
    } else {
      Err("No UI parent")
    }
  }
}
