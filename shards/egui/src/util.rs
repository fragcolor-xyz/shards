/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

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

pub fn into_shadow(v: &Var) -> Result<egui::epaint::Shadow, &'static str> {
  let tbl: TableVar = v.try_into()?;
  let extrusion: f32 = tbl
    .get_static("Extrusion")
    .ok_or("Extrusion missing")?
    .try_into()?;
  let color: egui::Color32 = into_color(tbl.get_static("Color").ok_or("Color missing")?)?;
  Ok(egui::epaint::Shadow { extrusion, color })
}

pub fn into_stroke(v: &Var) -> Result<egui::Stroke, &'static str> {
  let tbl: TableVar = v.try_into()?;
  let width: f32 = tbl.get_static("Width").ok_or("Width missing")?.try_into()?;
  let color: egui::Color32 = into_color(tbl.get_static("Color").ok_or("Color missing")?)?;
  Ok(egui::Stroke { width, color })
}
/*
/// Background color of widgets that must have a background fill,
/// such as the slider background, a checkbox background, or a radio button background.
///
/// Must never be [`Color32::TRANSPARENT`].
pub bg_fill: Color32,

/// Background color of widgets that can _optionally_ have a background fill, such as buttons.
///
/// May be [`Color32::TRANSPARENT`].
pub weak_bg_fill: Color32,

/// For surrounding rectangle of things that need it,
/// like buttons, the box of the checkbox, etc.
/// Should maybe be called `frame_stroke`.
pub bg_stroke: Stroke,

/// Button frames etc.
pub rounding: Rounding,

/// Stroke and text color of the interactive part of a component (button text, slider grab, check-mark, …).
pub fg_stroke: Stroke,

/// Make the frame this much larger.
pub expansion: f32, */
pub fn apply_widget_visuals(visuals: &mut WidgetVisuals, v: &Var) -> Result<(), &'static str> {
  let tbl: TableVar = v.try_into()?;
  if let Some(v) = tbl.get_static("BGFill") {
    visuals.bg_fill = into_color(v)?;
  }
  if let Some(v) = tbl.get_static("WeakBGFill") {
    visuals.weak_bg_fill = into_color(v)?;
  }
  if let Some(v) = tbl.get_static("BGStroke") {
    visuals.bg_stroke = into_stroke(v)?;
  }
  if let Some(v) = tbl.get_static("Rounding") {
    visuals.rounding = into_rounding(v)?;
  }
  if let Some(v) = tbl.get_static("FGStroke") {
    visuals.fg_stroke = into_stroke(v)?;
  }
  if let Some(v) = tbl.get_static("Expansion") {
    visuals.expansion = v.try_into()?;
  }
  Ok(())
}
