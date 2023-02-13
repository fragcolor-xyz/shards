/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

use crate::shards::gui::EGUI_CTX_TYPE;
use crate::shards::gui::EGUI_UI_SEQ_TYPE;
use crate::shards::gui::EGUI_UI_TYPE;
use crate::types::Context;
use crate::types::ExposedInfo;
use crate::types::ExposedTypes;
use crate::types::ParamVar;
use crate::types::Seq;
use crate::types::ShardsVar;
use crate::types::Type;
use crate::types::Var;
use crate::types::WireState;

pub(crate) fn update_seq<F>(seq_var: &mut ParamVar, inner: F) -> Result<(), &'static str>
where
  F: FnOnce(&mut Seq),
{
  let var = seq_var.get();
  let mut seq: Seq = var.try_into()?;
  inner(&mut seq);
  seq_var.set(seq.as_ref().into());

  Ok(())
}

pub(crate) fn with_object_stack_var<'a, T, F, R>(
  stack_var: &mut ParamVar,
  object: &T,
  object_type: &Type,
  inner: F,
) -> Result<R, &'static str>
where
  F: FnOnce() -> Result<R, &'static str>,
{
  // push
  unsafe {
    let var = Var::new_object_from_ptr(object as *const _, object_type);
    update_seq(stack_var, |seq| {
      seq.push(&var);
    })?;
  }

  let result = inner();

  // pop
  update_seq(stack_var, |seq| {
    seq.pop();
  })?;

  result
}

pub(crate) fn activate_ui_contents<'a>(
  context: &Context,
  input: &Var,
  ui: &mut egui::Ui,
  parents_stack_var: &mut ParamVar,
  contents: &ShardsVar,
) -> Result<Var, &'a str> {
  let mut output = Var::default();

  with_object_stack_var(parents_stack_var, ui, &EGUI_UI_TYPE, || {
    let wire_state = contents.activate(context, input, &mut output);
    if wire_state == WireState::Error {
      return Err("Failed to activate contents");
    }
    Ok(())
  })?;

  Ok(output)
}

pub(crate) fn expose_contents_variables(exposing: &mut ExposedTypes, contents: &ShardsVar) -> bool {
  if !contents.is_empty() {
    if let Some(contents_exposing) = contents.get_exposing() {
      for exp in contents_exposing {
        exposing.push(*exp);
      }
      true
    } else {
      false
    }
  } else {
    false
  }
}

pub(crate) fn get_current_context<'a>(
  context_stack_var: &ParamVar,
) -> Result<&'a mut egui::Context, &'a str> {
  let seq: Seq = context_stack_var.get().try_into()?;
  if !seq.is_empty() {
    Ok(Var::from_object_ptr_mut_ref(
      &seq[seq.len() - 1],
      &EGUI_CTX_TYPE,
    )?)
  } else {
    Err("Failed to get context, stack is empty")
  }
}

pub(crate) fn get_current_parent<'a>(
  parents_stack_var: &Var,
) -> Result<Option<&'a mut egui::Ui>, &'a str> {
  let seq: Seq = parents_stack_var.try_into()?;
  if !seq.is_empty() {
    let var = seq[seq.len() - 1];
    if var.is_none() {
      return Ok(None);
    }

    Ok(Some(Var::from_object_ptr_mut_ref(
      &seq[seq.len() - 1],
      &EGUI_UI_TYPE,
    )?))
  } else {
    Ok(None)
  }
}

pub(crate) fn require_parents(requiring: &mut ExposedTypes, parents_stack_var: &ParamVar) {
  let exp_info = ExposedInfo {
    exposedType: EGUI_UI_SEQ_TYPE,
    name: parents_stack_var.get_name(),
    help: cstr!("The parent UI objects.").into(),
    ..ExposedInfo::default()
  };
  requiring.push(exp_info);
}
