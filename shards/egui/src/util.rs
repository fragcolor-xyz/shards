/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

use crate::Context as UIContext;
use crate::CONTEXTS_NAME_CSTR;
use crate::EGUI_CTX_TYPE;
use crate::EGUI_UI_TYPE;
use crate::PARENTS_UI_NAME_CSTR;
use egui::style::WidgetVisuals;
use shards::types::Context;
use shards::types::ExposedInfo;
use shards::types::ExposedTypes;
use shards::types::ParamVar;
use shards::types::ShardsVar;
use shards::types::TableVar;
use shards::types::Type;
use shards::types::Var;
use shards::types::WireState;
use shards::util::get_or_var;
use shards::SHType_Object;

// Ugly but reasonable for now to fix crashes in users code
use std::panic;

pub fn with_possible_panic<F, R>(inner: F) -> Result<R, &'static str>
where
  F: FnOnce() -> R,
{
  // Store the original panic hook (e.g. Sentry)
  let original_hook = panic::take_hook();

  // Call the inner function with catch_unwind
  let result = panic::catch_unwind(panic::AssertUnwindSafe(inner));

  // Restore the original panic hook
  panic::set_hook(original_hook);

  // Handle the result
  match result {
    Ok(result) => Ok(result),
    Err(err) => {
      shlog_error!("Panic: {:?}", err);
      Err("Panic in UI code, avoid code that suspends while in UI context")
    }
  }
}

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
  let obj = Var::default();
  var.set_fast_unsafe(&obj);

  let result = inner();

  // Restore
  var.set_fast_unsafe(&prev);

  result
}

pub unsafe fn get_object_from_var_opt<'a, T>(
  var: &Var,
  obj_type: &Type,
) -> Result<Option<&'a mut T>, &'static str> {
  if var.valueType == SHType_Object {
    Ok(Some(Var::from_object_ptr_mut_ref(&var, obj_type)?))
  } else {
    Ok(None)
  }
}

pub unsafe fn get_object_from_var<'a, T>(
  var: &Var,
  obj_type: &Type,
) -> Result<&'a mut T, &'static str> {
  match get_object_from_var_opt(var, obj_type)? {
    Some(obj) => Ok(obj),
    None => Err("Object not set"),
  }
}

pub unsafe fn get_object_from_param_var<'a, T>(
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

#[inline(always)]
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
) -> Result<&'a mut UIContext, &'static str> {
  Ok(unsafe { get_object_from_var(&context_stack_var, &EGUI_CTX_TYPE)? })
}

pub fn get_current_context<'a>(
  context_stack_var: &ParamVar,
) -> Result<&'a mut UIContext, &'static str> {
  return get_current_context_from_var(context_stack_var.get());
}

pub fn get_current_parent_opt<'a>(
  parents_stack_var: &Var,
) -> Result<Option<&'a mut egui::Ui>, &'static str> {
  Ok(unsafe { get_object_from_var_opt(parents_stack_var, &EGUI_UI_TYPE)? })
}

pub fn get_parent_ui<'a>(parents_stack_var: &Var) -> Result<&'a mut egui::Ui, &'static str> {
  Ok(unsafe {
    match get_object_from_var_opt(parents_stack_var, &EGUI_UI_TYPE)? {
      Some(ui) => Ok(ui),
      None => Err("Parent UI missing"),
    }
  }?)
}

pub fn require_parents(requiring: &mut ExposedTypes) {
  let exp_info = ExposedInfo {
    exposedType: EGUI_UI_TYPE,
    name: PARENTS_UI_NAME_CSTR.as_ptr(),
    help: shccstr!("The parent UI objects."),
    ..ExposedInfo::default()
  };
  requiring.push(exp_info);
}

pub fn require_context(requiring: &mut ExposedTypes) {
  let exp_info = ExposedInfo {
    exposedType: EGUI_CTX_TYPE,
    name: CONTEXTS_NAME_CSTR.as_ptr(),
    help: shccstr!("The UI context."),
    ..ExposedInfo::default()
  };
  requiring.push(exp_info);
}

pub fn try_into_color(var: &Var) -> Result<egui::Color32, &'static str> {
  let color: shards::SHColor = var.try_into()?;
  Ok(egui::Color32::from_rgba_unmultiplied(
    color.r, color.g, color.b, color.a,
  ))
}

pub fn into_vec2(v: &Var) -> Result<egui::Vec2, &'static str> {
  let v: (f32, f32) = v.try_into()?;
  Ok(egui::vec2(v.0, v.1))
}

pub fn into_margin(v: &Var) -> Result<egui::Margin, &'static str> {
  let v: (f32, f32, f32, f32) = v.try_into()?;
  Ok(egui::Margin {
    left: v.0,
    right: v.1,
    top: v.2,
    bottom: v.3,
  })
}

pub fn into_color(v: &Var) -> Result<egui::Color32, &'static str> {
  let v: shards::SHColor = v.try_into()?;
  Ok(egui::Color32::from_rgba_premultiplied(v.r, v.g, v.b, v.a))
}

pub fn into_rounding(v: &Var) -> Result<egui::Rounding, &'static str> {
  let v: (f32, f32, f32, f32) = v.try_into()?;
  Ok(egui::Rounding {
    nw: v.0,
    ne: v.1,
    sw: v.2,
    se: v.3,
  })
}

pub fn into_shadow(v: &Var, ctx: &Context) -> Result<egui::epaint::Shadow, &'static str> {
  let tbl: TableVar = v.try_into()?;
  let spread: f32 =
    get_or_var(tbl.get_static("Spread").ok_or("Spread missing")?, ctx).try_into()?;
  let blur: f32 = get_or_var(tbl.get_static("Blur").ok_or("Blur missing")?, ctx).try_into()?;
  let spread: f32 =
    get_or_var(tbl.get_static("Spread").ok_or("Spread missing")?, ctx).try_into()?;
  let color: egui::Color32 = into_color(get_or_var(
    tbl.get_static("Color").ok_or("Color missing")?,
    ctx,
  ))?;
  Ok(egui::epaint::Shadow {
    spread,
    blur,
    offset: egui::Vec2::ZERO,
    color,
  })
}

pub fn into_stroke(v: &Var, ctx: &Context) -> Result<egui::Stroke, &'static str> {
  let tbl: TableVar = v.try_into()?;
  let width: f32 = get_or_var(tbl.get_static("Width").ok_or("Width missing")?, ctx).try_into()?;
  let color: egui::Color32 = into_color(get_or_var(
    tbl.get_static("Color").ok_or("Color missing")?,
    ctx,
  ))?;
  Ok(egui::Stroke { width, color })
}

pub fn apply_widget_visuals(
  visuals: &mut WidgetVisuals,
  v: &Var,
  ctx: &Context,
) -> Result<(), &'static str> {
  let tbl: TableVar = v.try_into()?;
  if let Some(v) = tbl.get_static("BGFill") {
    visuals.bg_fill = into_color(get_or_var(v, ctx))?;
  }
  if let Some(v) = tbl.get_static("WeakBGFill") {
    visuals.weak_bg_fill = into_color(get_or_var(v, ctx))?;
  }
  if let Some(v) = tbl.get_static("BGStroke") {
    visuals.bg_stroke = into_stroke(v, ctx)?;
  }
  if let Some(v) = tbl.get_static("Rounding") {
    visuals.rounding = into_rounding(get_or_var(v, ctx))?;
  }
  if let Some(v) = tbl.get_static("FGStroke") {
    visuals.fg_stroke = into_stroke(v, ctx)?;
  }
  if let Some(v) = tbl.get_static("Expansion") {
    visuals.expansion = v.try_into()?;
  }
  Ok(())
}
