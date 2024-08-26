/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

use super::Spinner;
use crate::util;
use crate::FLOAT_VAR_SLICE;
use crate::HELP_OUTPUT_EQUAL_INPUT;
use crate::HELP_VALUE_IGNORED;
use crate::PARENTS_UI_NAME;
use shards::shard::LegacyShard;
use shards::types::Context;
use shards::types::ExposedTypes;
use shards::types::OptionalString;
use shards::types::ParamVar;
use shards::types::Parameters;
use shards::types::Types;
use shards::types::Var;
use shards::types::ANY_TYPES;

lazy_static! {
  static ref SPINNER_PARAMETERS: Parameters = vec![
    (
      cstr!("Size"),
      shccstr!("Overrides the size of the spinner. This sets both the height and width, as the spinner is always square."),
      FLOAT_VAR_SLICE,
    )
      .into(),
  ];
}

impl Default for Spinner {
  fn default() -> Self {
    let mut parents = ParamVar::default();
    parents.set_name(PARENTS_UI_NAME);
    Self {
      parents,
      requiring: Vec::new(),
      size: ParamVar::default(),
    }
  }
}

impl LegacyShard for Spinner {
  fn registerName() -> &'static str
  where
    Self: Sized,
  {
    cstr!("UI.Spinner")
  }

  fn hash() -> u32
  where
    Self: Sized,
  {
    compile_time_crc32::crc32!("UI.Spinner-rust-0x20200101")
  }

  fn name(&mut self) -> &str {
    "UI.Spinner"
  }

  fn help(&mut self) -> OptionalString {
    OptionalString(shccstr!("A spinner widget used to indicate loading."))
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

  fn parameters(&mut self) -> Option<&Parameters> {
    Some(&SPINNER_PARAMETERS)
  }

  fn setParam(&mut self, index: i32, value: &Var) -> Result<(), &str> {
    match index {
      0 => self.size.set_param(value),
      _ => Err("Invalid parameter index"),
    }
  }

  fn getParam(&mut self, index: i32) -> Var {
    match index {
      0 => self.size.get_param(),
      _ => Var::default(),
    }
  }

  fn requiredVariables(&mut self) -> Option<&ExposedTypes> {
    self.requiring.clear();

    // Add UI.Parents to the list of required variables
    util::require_parents(&mut self.requiring);

    Some(&self.requiring)
  }

  fn warmup(&mut self, context: &Context) -> Result<(), &str> {
    self.parents.warmup(context);

    self.size.warmup(context);

    Ok(())
  }

  fn cleanup(&mut self, ctx: Option<&Context>) -> Result<(), &str> {
    self.size.cleanup(ctx);

    self.parents.cleanup(ctx);

    Ok(())
  }

  fn activate(&mut self, _context: &Context, input: &Var) -> Result<Option<Var>, &str> {
    if let Some(ui) = util::get_current_parent_opt(self.parents.get())? {
      let mut spinner = egui::Spinner::new();

      let size = self.size.get();
      if !size.is_none() {
        spinner = spinner.size(size.try_into()?);
      }

      ui.add(spinner);

      Ok(None)
    } else {
      Err("No UI parent")
    }
  }
}
