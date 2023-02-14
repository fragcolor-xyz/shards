/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

use super::ColorInput;
use crate::shard::Shard;
use crate::shards::gui::util;
use crate::shards::gui::COLOR_VAR_OR_NONE_SLICE;
use crate::shards::gui::HELP_VALUE_IGNORED;
use crate::shards::gui::PARENTS_UI_NAME;
use crate::shardsc;
use crate::types::common_type;
use crate::types::Context;
use crate::types::ExposedInfo;
use crate::types::ExposedTypes;
use crate::types::InstanceData;
use crate::types::OptionalString;
use crate::types::ParamVar;
use crate::types::Parameters;
use crate::types::Type;
use crate::types::Types;
use crate::types::Var;
use crate::types::COLOR_TYPES;
use crate::types::NONE_TYPES;
use std::cmp::Ordering;
use std::ffi::CStr;

lazy_static! {
  static ref COLORINPUT_PARAMETERS: Parameters = vec![(
    cstr!("Variable"),
    shccstr!("The variable that holds the input value."),
    COLOR_VAR_OR_NONE_SLICE,
  )
    .into(),];
}

impl Default for ColorInput {
  fn default() -> Self {
    let mut parents = ParamVar::default();
    parents.set_name(PARENTS_UI_NAME);
    Self {
      parents,
      requiring: Vec::new(),
      variable: ParamVar::default(),
      exposing: Vec::new(),
      should_expose: false,
      tmp: Default::default(),
    }
  }
}

impl Shard for ColorInput {
  fn registerName() -> &'static str
  where
    Self: Sized,
  {
    cstr!("UI.ColorInput")
  }

  fn hash() -> u32
  where
    Self: Sized,
  {
    compile_time_crc32::crc32!("UI.ColorInput-rust-0x20200101")
  }

  fn name(&mut self) -> &str {
    "UI.ColorInput"
  }

  fn help(&mut self) -> OptionalString {
    OptionalString(shccstr!("A widget where a color can be selected."))
  }

  fn inputTypes(&mut self) -> &Types {
    &NONE_TYPES
  }

  fn inputHelp(&mut self) -> OptionalString {
    *HELP_VALUE_IGNORED
  }

  fn outputTypes(&mut self) -> &Types {
    &COLOR_TYPES
  }

  fn outputHelp(&mut self) -> OptionalString {
    OptionalString(shccstr!("The selected color."))
  }

  fn parameters(&mut self) -> Option<&Parameters> {
    Some(&COLORINPUT_PARAMETERS)
  }

  fn setParam(&mut self, index: i32, value: &Var) -> Result<(), &str> {
    match index {
      0 => Ok(self.variable.set_param(value)),
      _ => Err("Invalid parameter index"),
    }
  }

  fn getParam(&mut self, index: i32) -> Var {
    match index {
      0 => self.variable.get_param(),
      _ => Var::default(),
    }
  }

  fn hasCompose() -> bool {
    true
  }

  fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
    if self.variable.is_variable() {
      self.should_expose = true; // assume we expose a new variable

      let shared: ExposedTypes = data.shared.into();
      for var in shared {
        let (a, b) = unsafe {
          (
            CStr::from_ptr(var.name),
            CStr::from_ptr(self.variable.get_name()),
          )
        };
        if CStr::cmp(a, b) == Ordering::Equal {
          self.should_expose = false;
          if var.exposedType.basicType != shardsc::SHType_Color {
            return Err("ColorInput: color variable required.");
          }
          break;
        }
      }
    }

    Ok(common_type::color)
  }

  fn exposedVariables(&mut self) -> Option<&ExposedTypes> {
    if self.variable.is_variable() && self.should_expose {
      self.exposing.clear();

      let exp_info = ExposedInfo {
        exposedType: common_type::color,
        name: self.variable.get_name(),
        help: cstr!("The exposed color variable").into(),
        ..ExposedInfo::default()
      };

      self.exposing.push(exp_info);
      Some(&self.exposing)
    } else {
      None
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

    self.variable.warmup(ctx);

    if self.should_expose {
      self.variable.get_mut().valueType = common_type::color.basicType;
    }

    Ok(())
  }

  fn cleanup(&mut self) -> Result<(), &str> {
    self.variable.cleanup();

    self.parents.cleanup();

    Ok(())
  }

  fn activate(&mut self, _context: &Context, _input: &Var) -> Result<Var, &str> {
    if let Some(ui) = util::get_current_parent(self.parents.get())? {
      let color = if self.variable.is_variable() {
        unsafe { &mut self.variable.get_mut().payload.__bindgen_anon_1.colorValue }
      } else {
        &mut self.tmp
      };
      let mut srgba = [color.r, color.g, color.b, color.a];
      ui.color_edit_button_srgba_unmultiplied(&mut srgba);
      color.r = srgba[0];
      color.g = srgba[1];
      color.b = srgba[2];
      color.a = srgba[3];

      Ok((*color).into())
    } else {
      Err("No UI parent")
    }
  }
}
