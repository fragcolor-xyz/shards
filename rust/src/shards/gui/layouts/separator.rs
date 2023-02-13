/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

use super::Separator;
use crate::shard::Shard;
use crate::shards::gui::util;
use crate::shards::gui::HELP_OUTPUT_EQUAL_INPUT;
use crate::shards::gui::HELP_VALUE_IGNORED;
use crate::shards::gui::PARENTS_UI_NAME;
use crate::types::Context;
use crate::types::ExposedTypes;
use crate::types::OptionalString;
use crate::types::ParamVar;
use crate::types::Types;
use crate::types::Var;
use crate::types::ANY_TYPES;

impl Default for Separator {
  fn default() -> Self {
    let mut parents = ParamVar::default();
    parents.set_name(PARENTS_UI_NAME);
    Self {
      parents,
      requiring: Vec::new(),
    }
  }
}

impl Shard for Separator {
  fn registerName() -> &'static str
  where
    Self: Sized,
  {
    cstr!("UI.Separator")
  }

  fn hash() -> u32
  where
    Self: Sized,
  {
    compile_time_crc32::crc32!("UI.Separator-rust-0x20200101")
  }

  fn name(&mut self) -> &str {
    "UI.Separator"
  }

  fn help(&mut self) -> OptionalString {
    OptionalString(shccstr!(
      "A visual separator. A horizontal or vertical line (depending on the layout)."
    ))
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
      ui.separator();

      Ok(*input)
    } else {
      Err("No UI parent")
    }
  }
}
