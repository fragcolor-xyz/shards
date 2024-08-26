/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

use super::RadioButton;
use crate::util;
use crate::widgets::text_util;
use crate::ANY_TABLE_SLICE;
use crate::ANY_VAR_SLICE;
use crate::HELP_VALUE_IGNORED;
use crate::PARENTS_UI_NAME;
use shards::shard::LegacyShard;
use shards::types::common_type;
use shards::types::Context;
use shards::types::ExposedInfo;
use shards::types::ExposedTypes;
use shards::types::InstanceData;
use shards::types::OptionalString;
use shards::types::ParamVar;
use shards::types::Parameters;
use shards::types::Type;
use shards::types::Types;
use shards::types::Var;
use shards::types::ANY_TYPES;
use shards::types::BOOL_TYPES;
use shards::types::NONE_TYPES;
use shards::types::STRING_OR_NONE_SLICE;
use std::cmp::Ordering;
use std::ffi::CStr;

lazy_static! {
  static ref RADIOBUTTON_PARAMETERS: Parameters = vec![
    (
      cstr!("Label"),
      shccstr!("The text label of this radio button."),
      STRING_OR_NONE_SLICE,
    )
      .into(),
    (
      cstr!("Variable"),
      shccstr!("The variable that holds the input value."),
      ANY_VAR_SLICE,
    )
      .into(),
    (
      cstr!("Value"),
      shccstr!("The value to compare with."),
      &ANY_TYPES[..],
    )
      .into(),
    (cstr!("Style"), shccstr!("The text style."), ANY_TABLE_SLICE,).into(),
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
      style: ParamVar::default(),
      exposing: Vec::new(),
      should_expose: false,
    }
  }
}

impl LegacyShard for RadioButton {
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

  fn help(&mut self) -> OptionalString {
    OptionalString(shccstr!(
      "A radio button for selecting a value amongst multiple choices."
    ))
  }

  fn inputTypes(&mut self) -> &Types {
    &NONE_TYPES
  }

  fn inputHelp(&mut self) -> OptionalString {
    *HELP_VALUE_IGNORED
  }

  fn outputTypes(&mut self) -> &Types {
    &BOOL_TYPES
  }

  fn outputHelp(&mut self) -> OptionalString {
    OptionalString(shccstr!(
      "Indicates whether the radio button was clicked during this frame."
    ))
  }

  fn parameters(&mut self) -> Option<&Parameters> {
    Some(&RADIOBUTTON_PARAMETERS)
  }

  fn setParam(&mut self, index: i32, value: &Var) -> Result<(), &str> {
    match index {
      0 => self.label.set_param(value),
      1 => self.variable.set_param(value),
      2 => Ok(self.value = *value),
      3 => self.style.set_param(value),
      _ => Err("Invalid parameter index"),
    }
  }

  fn getParam(&mut self, index: i32) -> Var {
    match index {
      0 => self.label.get_param(),
      1 => self.variable.get_param(),
      2 => self.value,
      3 => self.style.get_param(),
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
        help: shccstr!("The exposed variable"),
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
    util::require_parents(&mut self.requiring);

    Some(&self.requiring)
  }

  fn warmup(&mut self, ctx: &Context) -> Result<(), &str> {
    self.parents.warmup(ctx);

    self.label.warmup(ctx);
    self.variable.warmup(ctx);
    self.style.warmup(ctx);

    if self.should_expose {
      self.variable.get_mut().valueType = self.value.valueType;
    }

    Ok(())
  }

  fn cleanup(&mut self, ctx: Option<&Context>) -> Result<(), &str> {
    self.style.cleanup(ctx);
    self.variable.cleanup(ctx);
    self.label.cleanup(ctx);

    self.parents.cleanup(ctx);

    Ok(())
  }

  fn activate(&mut self, _context: &Context, _input: &Var) -> Result<Option<Var>, &str> {
    if let Some(ui) = util::get_current_parent_opt(self.parents.get())? {
      let label: &str = self.label.get().try_into()?;
      let mut text = egui::RichText::new(label);

      let style = self.style.get();
      if !style.is_none() {
        text = text_util::get_styled_text(text, &style.try_into()?)?;
      }

      let variable = self.variable.get();
      if variable.is_none() {
        return Err("Variable cannot be None");
      }

      let radio = egui::RadioButton::new(*variable == self.value, text);
      let response = ui.add(radio);
      if response.clicked() {
        if self.variable.is_variable() {
          self.variable.set_cloning(&self.value);
        }

        // radio button clicked during this frame
        return Ok(Some(true.into()));
      }

      // radio button not clicked during this frame
      Ok(Some(false.into()))
    } else {
      Err("No UI parent")
    }
  }
}
