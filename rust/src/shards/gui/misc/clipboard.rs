/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

use super::GetClipboard;
use super::SetClipboard;
use crate::core::cloneVar;
use crate::shard::Shard;
use crate::shards::gui::util;
use crate::shards::gui::PARENTS_UI_NAME;
use crate::types::ClonedVar;
use crate::types::Context;
use crate::types::ExposedTypes;
use crate::types::ParamVar;
use crate::types::Types;
use crate::types::Var;
use crate::types::NONE_TYPES;
use crate::types::STRING_TYPES;

impl Default for GetClipboard {
  fn default() -> Self {
    let mut parents = ParamVar::default();
    parents.set_name(PARENTS_UI_NAME);
    Self {
      parents,
      requiring: Vec::new(),
    }
  }
}

impl Shard for GetClipboard {
  fn registerName() -> &'static str
  where
    Self: Sized,
  {
    cstr!("UI.GetClipboard")
  }

  fn hash() -> u32
  where
    Self: Sized,
  {
    compile_time_crc32::crc32!("UI.GetClipboard-rust-0x20200101")
  }

  fn name(&mut self) -> &str {
    "UI.GetClipboard"
  }

  fn inputTypes(&mut self) -> &Types {
    &NONE_TYPES
  }

  fn outputTypes(&mut self) -> &Types {
    &STRING_TYPES
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

  fn activate(&mut self, _context: &Context, _input: &Var) -> Result<Var, &str> {
    if let Some(ui) = util::get_current_parent(*self.parents.get())? {
      let mut ret: Var = Default::default();
      // FIXME currently only work for a single egui frame. copied_text is at the start of the next frame.
      let tmp = Var::ephemeral_string(&ui.output().copied_text);
      cloneVar(&mut ret, &tmp);
      Ok(ret)
    } else {
      Err("No UI parent")
    }
  }
}

impl Default for SetClipboard {
  fn default() -> Self {
    let mut parents = ParamVar::default();
    parents.set_name(PARENTS_UI_NAME);
    Self {
      parents,
      requiring: Vec::new(),
    }
  }
}

impl Shard for SetClipboard {
  fn registerName() -> &'static str
  where
    Self: Sized,
  {
    cstr!("UI.SetClipboard")
  }

  fn hash() -> u32
  where
    Self: Sized,
  {
    compile_time_crc32::crc32!("UI.SetClipboard-rust-0x20200101")
  }

  fn name(&mut self) -> &str {
    "UI.SetClipboard"
  }

  fn inputTypes(&mut self) -> &Types {
    &STRING_TYPES
  }

  fn outputTypes(&mut self) -> &Types {
    &STRING_TYPES
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
    if let Some(ui) = util::get_current_parent(*self.parents.get())? {
      ui.output().copied_text = input.try_into()?;

      Ok(*input)
    } else {
      Err("No UI parent")
    }
  }
}
