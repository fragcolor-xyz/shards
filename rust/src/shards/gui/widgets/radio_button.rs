/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

use super::RadioButton;
use crate::core::cloneVar;
use crate::shard::Shard;
use crate::shards::gui::util;
use crate::shards::gui::ANY_VAR_SLICE;
use crate::shards::gui::EGUI_UI_SEQ_TYPE;
use crate::shards::gui::PARENTS_UI_NAME;
use crate::types::common_type;
use crate::types::Context;
use crate::types::ExposedInfo;
use crate::types::ExposedTypes;
use crate::types::InstanceData;
use crate::types::ParamVar;
use crate::types::Parameters;
use crate::types::Type;
use crate::types::Types;
use crate::types::Var;
use crate::types::ANY_TYPES;
use crate::types::BOOL_TYPES;
use crate::types::NONE_TYPES;
use crate::types::STRING_OR_NONE_SLICE;
use std::cmp::Ordering;
use std::ffi::CStr;

lazy_static! {
  static ref RADIOBUTTON_PARAMETERS: Parameters = vec![
    (
      cstr!("Label"),
      cstr!("The text label of this radio button."),
      STRING_OR_NONE_SLICE,
    )
      .into(),
    (
      cstr!("Variable"),
      cstr!("The variable that holds the input value."),
      ANY_VAR_SLICE,
    )
      .into(),
    (
      cstr!("Value"),
      cstr!("The value to compare with."),
      &ANY_TYPES[..],
    )
      .into(),
  ];
}

impl Default for RadioButton {
  fn default() -> Self {
    let mut parents = ParamVar::default();
    parents.set_name(PARENTS_UI_NAME);
    Self {
      parents,
      requiring: Vec::new(),
      label: ParamVar::default(),
      variable: ParamVar::default(),
      value: Var::default(),
      exposing: Vec::new(),
      should_expose: false,
    }
  }
}

impl Shard for RadioButton {
  fn registerName() -> &'static str
  where
    Self: Sized,
  {
    cstr!("UI.RadioButton")
  }

  fn hash() -> u32
  where
    Self: Sized,
  {
    compile_time_crc32::crc32!("UI.RadioButton-rust-0x20200101")
  }

  fn name(&mut self) -> &str {
    "UI.RadioButton"
  }

  fn inputTypes(&mut self) -> &Types {
    &NONE_TYPES
  }

  fn outputTypes(&mut self) -> &Types {
    &BOOL_TYPES
  }

  fn parameters(&mut self) -> Option<&Parameters> {
    Some(&RADIOBUTTON_PARAMETERS)
  }

  fn setParam(&mut self, index: i32, value: &Var) -> Result<(), &str> {
    match index {
      0 => Ok(self.label.set_param(value)),
      1 => Ok(self.variable.set_param(value)),
      2 => Ok(self.value = *value),
      _ => Err("Invalid parameter index"),
    }
  }

  fn getParam(&mut self, index: i32) -> Var {
    match index {
      0 => self.label.get_param(),
      1 => self.variable.get_param(),
      2 => self.value,
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
        if CStr::cmp(&a, &b) == Ordering::Equal {
          self.should_expose = false;
          break;
        }
      }
    }

    Ok(common_type::any)
  }

  fn exposedVariables(&mut self) -> Option<&ExposedTypes> {
    if self.variable.is_variable() && self.should_expose {
      self.exposing.clear();

      let exp_info = ExposedInfo {
        exposedType: Type {
          basicType: self.value.valueType,
          ..Default::default()
        },
        name: self.variable.get_name(),
        help: cstr!("The exposed variable").into(),
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
    let exp_info = ExposedInfo {
      exposedType: EGUI_UI_SEQ_TYPE,
      name: self.parents.get_name(),
      help: cstr!("The parent UI objects.").into(),
      ..ExposedInfo::default()
    };
    self.requiring.push(exp_info);

    Some(&self.requiring)
  }

  fn warmup(&mut self, ctx: &Context) -> Result<(), &str> {
    self.parents.warmup(ctx);

    self.label.warmup(ctx);
    self.variable.warmup(ctx);

    if self.should_expose {
      self.variable.get_mut().valueType = self.value.valueType;
    }

    Ok(())
  }

  fn cleanup(&mut self) -> Result<(), &str> {
    self.variable.cleanup();
    self.label.cleanup();

    self.parents.cleanup();

    Ok(())
  }

  fn activate(&mut self, _context: &Context, _input: &Var) -> Result<Var, &str> {
    if let Some(ui) = util::get_current_parent(*self.parents.get())? {
      let label: &str = self.label.get().try_into()?;
      let variable = self.variable.get();
      if variable.is_none() {
        return Err("Variable cannot be None");
      }
      let radio = egui::RadioButton::new(*variable == self.value, label);
      let response = ui.add(radio);
      if response.clicked() {
        if self.variable.is_variable() {
          cloneVar(self.variable.get_mut(), &self.value);
        }

        // radio button clicked during this frame
        return Ok(true.into());
      }

      // radio button not clicked during this frame
      Ok(false.into())
    } else {
      Err("No UI parent")
    }
  }
}
