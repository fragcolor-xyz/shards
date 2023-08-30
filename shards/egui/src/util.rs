/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

use crate::CONTEXTS_NAME_CSTR;
use crate::EGUI_CTX_TYPE;
use crate::EGUI_UI_TYPE;
use crate::PARENTS_UI_NAME_CSTR;
use shards::types::Context;
use shards::types::ExposedInfo;
use shards::types::ExposedTypes;
use shards::types::ParamVar;
use shards::types::ShardsVar;
use shards::types::Type;
use shards::types::Var;
use shards::types::WireState;
use shards::SHType_Object;

pub fn with_object_var<T, F, R>(
  var: &mut ParamVar,
  object: &T,
  object_type: &Type,
  inner: F,
) -> Result<R, &'static str>
where
  F: FnOnce() -> Result<R, &'static str>,
{
  let prev = *var.get();

  // Swap
  unsafe {
    let obj = Var::new_object_from_ptr(object as *const _, object_type);
    var.set_fast_unsafe(&obj);
  }

  let result = inner();

  // Restore
  var.set_fast_unsafe(&prev);

  result
}

pub fn with_none_var<F, R>(var: &mut ParamVar, inner: F) -> Result<R, &'static str>
where
  F: FnOnce() -> Result<R, &'static str>,
{
  let prev = *var.get();

  // Swap
  unsafe {
    let obj = Var::default();
    var.set_fast_unsafe(&obj);
  }

  let result = inner();

  // Restore
  var.set_fast_unsafe(&prev);

  result
}

pub fn get_object_from_var_opt<'a, T>(
  var: &Var,
  obj_type: &Type,
) -> Result<Option<&'a mut T>, &'static str> {
  if var.valueType == SHType_Object {
    Ok(Some(Var::from_object_ptr_mut_ref(&var, obj_type)?))
  } else {
    Ok(None)
  }
}

pub fn get_object_from_var<'a, T>(var: &Var, obj_type: &Type) -> Result<&'a mut T, &'static str> {
  match get_object_from_var_opt(var, obj_type)? {
    Some(obj) => Ok(obj),
    None => Err("Object not set"),
  }
}

pub fn get_object_from_param_var<'a, T>(
  var: &ParamVar,
  obj_type: &Type,
) -> Result<&'a mut T, &'static str> {
  get_object_from_var(var.get(), obj_type)
}

// TODO: Remove
pub fn with_object_stack_var<'a, T, F, R>(
  stack_var: &mut ParamVar,
  object: &T,
  object_type: &Type,
  inner: F,
) -> Result<R, &'static str>
where
  F: FnOnce() -> Result<R, &'static str>,
{
  with_object_var(stack_var, object, object_type, inner)
}

pub fn activate_ui_contents(
  context: &Context,
  input: &Var,
  ui: &mut egui::Ui,
  parents_stack_var: &mut ParamVar,
  contents: &ShardsVar,
) -> Result<Var, &'static str> {
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
  get_object_from_var(&context_stack_var, &EGUI_CTX_TYPE)
}

pub fn get_current_context<'a>(
  context_stack_var: &ParamVar,
) -> Result<&'a mut egui::Context, &'a str> {
  return get_current_context_from_var(context_stack_var.get());
}

pub fn get_current_parent_opt<'a>(
  parents_stack_var: &Var,
) -> Result<Option<&'a mut egui::Ui>, &'static str> {
  get_object_from_var_opt(parents_stack_var, &EGUI_UI_TYPE)
}

pub fn get_parent_ui<'a>(parents_stack_var: &Var) -> Result<&'a mut egui::Ui, &'static str> {
  match get_object_from_var_opt(parents_stack_var, &EGUI_UI_TYPE)? {
    Some(ui) => Ok(ui),
    None => Err("Parent UI missing"),
  }
}

pub fn require_parents(requiring: &mut ExposedTypes) {
  let exp_info = ExposedInfo {
    exposedType: EGUI_UI_TYPE,
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

pub fn try_into_color(var: &Var) -> Result<egui::Color32, &'static str> {
  let color: shards::SHColor = var.try_into()?;
  Ok(egui::Color32::from_rgba_unmultiplied(color.r, color.g, color.b, color.a))
}
