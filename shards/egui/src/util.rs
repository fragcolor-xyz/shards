/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

use std::ffi::CStr;

use crate::CONTEXTS_NAME;
use crate::CONTEXTS_NAME_CSTR;
use crate::EGUI_CTX_TYPE;
use crate::EGUI_UI_SEQ_TYPE;
use crate::EGUI_UI_TYPE;
use crate::PARENTS_UI_NAME;
use crate::PARENTS_UI_NAME_CSTR;
use shards::types::Context;
use shards::types::ExposedInfo;
use shards::types::ExposedTypes;
use shards::types::ParamVar;
use shards::types::Seq;
use shards::types::ShardsVar;
use shards::types::Type;
use shards::types::Var;
use shards::types::WireState;
use shards::util;

pub fn update_seq<F>(seq_var: &mut ParamVar, inner: F) -> Result<(), &'static str>
where
  F: FnOnce(&mut Seq),
{
  let var = seq_var.get();
  let mut seq: Seq = var.try_into()?;
  inner(&mut seq);
  seq_var.set_fast_unsafe(&seq.as_ref().into());
  Ok(())
}

pub fn with_object_stack_var<'a, T, F, R>(
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

pub fn with_object_stack_var_pass_stack_var<'a, T, F, R>(
  stack_var: &mut ParamVar,
  object: &mut T,
  object_type: &Type,
  inner: F,
) -> Result<R, &'static str>
where
  F: FnOnce(&mut ParamVar) -> Result<R, &'static str>,
{
  // push
  unsafe {
    let var = Var::new_object_from_ptr(object as *const _, object_type);
    update_seq(stack_var, |seq| {
      seq.push(&var);
    })?;
  }

  let result = inner(stack_var);

  // pop
  update_seq(stack_var, |seq| {
    seq.pop();
  })?;

  result
}

pub fn activate_ui_contents<'a>(
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

pub fn expose_contents_variables(exposing: &mut ExposedTypes, contents: &ShardsVar) -> bool {
  shards::util::expose_shards_contents(exposing, contents)
}

pub fn get_current_context_from_var<'a>(
  context_stack_var: &Var,
) -> Result<&'a mut egui::Context, &'a str> {
  let seq: Seq = context_stack_var.try_into()?;
  if !seq.is_empty() {
    Ok(Var::from_object_ptr_mut_ref(
      &seq[seq.len() - 1],
      &EGUI_CTX_TYPE,
    )?)
  } else {
    Err("Failed to get context, stack is empty")
  }
}

pub fn get_current_context<'a>(
  context_stack_var: &ParamVar,
) -> Result<&'a mut egui::Context, &'a str> {
  return get_current_context_from_var(context_stack_var.get());
}

pub fn get_current_parent<'a>(
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

pub fn require_parents(requiring: &mut ExposedTypes) {
  let exp_info = ExposedInfo {
    exposedType: EGUI_UI_SEQ_TYPE,
    name: PARENTS_UI_NAME_CSTR.as_ptr(),
    help: cstr!("The parent UI objects.").into(),
    ..ExposedInfo::default()
  };
  requiring.push(exp_info);
}

pub fn require_context(requiring: &mut ExposedTypes) {
  let exp_info = ExposedInfo {
    exposedType: EGUI_CTX_TYPE,
    name: CONTEXTS_NAME_CSTR.as_ptr(),
    help: cstr!("The UI context.").into(),
    ..ExposedInfo::default()
  };
  requiring.push(exp_info);
}
