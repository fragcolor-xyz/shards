/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

use super::Label;
use crate::shard::Shard;
use crate::shards::gui::{EGUI_UI_TYPE, PARENT_UI_NAME};
use crate::types::Context;
use crate::types::ExposedInfo;
use crate::types::ExposedTypes;
use crate::types::ParamVar;
use crate::types::Types;
use crate::types::Var;
use crate::types::STRING_TYPES;
use egui::Ui;

impl Default for Label {
  fn default() -> Self {
    let mut ui_ctx = ParamVar::new(().into());
    ui_ctx.set_name(PARENT_UI_NAME);
    Label {
      parent: ui_ctx,
      requiring: Vec::new(),
    }
  }
}

impl Shard for Label {
  fn registerName() -> &'static str
  where
    Self: Sized,
  {
    cstr!("UI.Label")
  }

  fn hash() -> u32
  where
    Self: Sized,
  {
    compile_time_crc32::crc32!("UI.Label-rust-0x20200101")
  }

  fn name(&mut self) -> &str {
    "UI.Label"
  }

  fn inputTypes(&mut self) -> &Types {
    &STRING_TYPES
  }

  fn outputTypes(&mut self) -> &Types {
    &STRING_TYPES
  }

  fn requiredVariables(&mut self) -> Option<&ExposedTypes> {
    self.requiring.clear();

    // Add GUI.Context to the list of required variables
    let exp_info = ExposedInfo {
      exposedType: EGUI_UI_TYPE,
      name: self.parent.get_name(),
      help: cstr!("The parent UI object.").into(),
      ..ExposedInfo::default()
    };
    self.requiring.push(exp_info);

    Some(&self.requiring)
  }

  fn warmup(&mut self, context: &Context) -> Result<(), &str> {
    self.parent.warmup(context);
    Ok(())
  }

  fn cleanup(&mut self) -> Result<(), &str> {
    self.parent.cleanup();
    Ok(())
  }

  fn activate(&mut self, _context: &Context, input: &Var) -> Result<Var, &str> {
    let ui: &mut Ui = Var::from_object_ptr_mut_ref(self.parent.get(), &EGUI_UI_TYPE)?;

    let text: &str = input.as_ref().try_into()?;
    ui.label(text);

    Ok(*input)
  }
}
