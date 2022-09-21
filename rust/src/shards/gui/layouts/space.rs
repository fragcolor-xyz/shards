/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

use std::convert::TryInto;

use super::Space;
use crate::shard::Shard;
use crate::shards::gui::FLOAT_VAR_SLICE;
use crate::shards::gui::util;
use crate::shards::gui::PARENTS_UI_NAME;
use crate::types::Context;
use crate::types::ExposedTypes;
use crate::types::OptionalString;
use crate::types::ParamVar;
use crate::types::Parameters;
use crate::types::Types;
use crate::types::Var;
use crate::types::ANY_TYPES;

lazy_static! {
    static ref SPACE_PARAMETERS: Parameters = vec![
      (
        cstr!("Amount"),
        cstr!("The amount of space to insert."),
        FLOAT_VAR_SLICE
      )
        .into(),
    ];
}

impl Default for Space {
  fn default() -> Self {
    let mut parents = ParamVar::default();
    parents.set_name(PARENTS_UI_NAME);
    Self {
      parents,
      requiring: Vec::new(),
      amount: ParamVar::default(),
    }
  }
}

impl Shard for Space {
  fn registerName() -> &'static str
  where
    Self: Sized,
  {
    cstr!("UI.Space")
  }

  fn hash() -> u32
  where
    Self: Sized,
  {
    compile_time_crc32::crc32!("UI.Space-rust-0x20200101")
  }

  fn name(&mut self) -> &str {
    "UI.Space"
  }

  fn help(&mut self) -> OptionalString {
    OptionalString(shccstr!(
      "Inserts an empty space before the next widget. The direction will depend on the layout."
    ))
  }

  fn inputTypes(&mut self) -> &Types {
    &ANY_TYPES
  }

  fn inputHelp(&mut self) -> OptionalString {
    OptionalString(shccstr!("The value is ignored."))
  }

  fn outputTypes(&mut self) -> &Types {
    &ANY_TYPES
  }

  fn outputHelp(&mut self) -> OptionalString {
    OptionalString(shccstr!("The output of this shard will be its input."))
  }

  fn parameters(&mut self) -> Option<&Parameters> {
    Some(&SPACE_PARAMETERS)
  }

  fn setParam(&mut self, index: i32, value: &Var) -> Result<(), &str> {
    match index {
      0 => Ok(self.amount.set_param(value)),
      _ => Err("Invalid parameter index"),
    }
  }

  fn getParam(&mut self, index: i32) -> Var {
    match index {
      0 => self.amount.get_param(),
      _ => Var::default(),
    }
  }

  fn requiredVariables(&mut self) -> Option<&ExposedTypes> {
    self.requiring.clear();

    // Add UI.Parents to the list of required variables
    util::require_parents(&mut self.requiring, &self.parents);

    Some(&self.requiring)
  }

  fn warmup(&mut self, ctx: &Context) -> Result<(), &str> {
    self.parents.warmup(ctx);
    self.amount.warmup(ctx);

    Ok(())
  }

  fn cleanup(&mut self) -> Result<(), &str> {
    self.amount.cleanup();
    self.parents.cleanup();

    Ok(())
  }

  fn activate(&mut self, _context: &Context, input: &Var) -> Result<Var, &str> {
    if let Some(ui) = util::get_current_parent(*self.parents.get())? {
      ui.add_space(self.amount.get().try_into()?);

      Ok(*input)
    } else {
      Err("No UI parent")
    }
  }
}
