/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

use crate::shards::gui::EGUI_UI_TYPE;
use crate::types::Context;
use crate::types::ParamVar;
use crate::types::Seq;
use crate::types::ShardsVar;
use crate::types::Var;
use crate::types::WireState;

pub(crate) fn activate_ui_contents<'a>(
  context: &Context,
  input: &Var,
  ui: &mut egui::Ui,
  parents: &mut ParamVar,
  contents: &mut ShardsVar,
) -> Result<Var, &'a str> {
  // pass the ui parent to the inner shards
  let var = parents.get();
  let mut seq = if var.is_seq() {
    let seq: Seq = var.as_ref().try_into()?;
    // crate::core::logLevel(0, &format!("Parent count={}\0", seq.len()));
    seq
  } else {
    // crate::core::logLevel(0, &format!("Empty parents\0"));
    Seq::new()
  };
  unsafe {
    let var = Var::new_object_from_ptr(ui as *const _, &EGUI_UI_TYPE);
    seq.push(var);
    parents.set(seq.as_ref().into());
  }

  let mut output = Var::default();
  let state = contents.activate(context, input, &mut output);

  // restore the previous ui parent
  seq.pop();
  parents.set(seq.as_ref().into());

  if state == WireState::Error {
    return Err("Failed to activate contents");
  }

  Ok(output)
}

pub(crate) fn get_current_parent<'a>(parents: Var) -> Result<Option<&'a mut egui::Ui>, &'a str> {
  let ui;
  if parents.is_seq() {
    let seq: Seq = parents.try_into()?;
    if seq.len() > 0 {
      ui = Some(Var::from_object_ptr_mut_ref(
        seq[seq.len() - 1],
        &EGUI_UI_TYPE,
      )?);
    } else {
      ui = None;
    }
  } else {
    ui = None;
  }
  Ok(ui)
}
