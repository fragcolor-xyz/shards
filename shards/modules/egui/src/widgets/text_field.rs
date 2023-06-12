/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

use super::TextField;
use shards::shard::Shard;
use crate::MutVarTextBuffer;
use crate::VarTextBuffer;
use crate::util;
use crate::HELP_VALUE_IGNORED;
use crate::PARENTS_UI_NAME;
use crate::STRING_VAR_SLICE;
use shards::shardsc;
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
use shards::types::BOOL_TYPES_SLICE;
use shards::types::NONE_TYPES;
use shards::types::STRING_TYPES;
use std::cmp::Ordering;
use std::ffi::CStr;

lazy_static! {
  static ref TEXTINPUT_PARAMETERS: Parameters = vec![
    (
      cstr!("Variable"),
      shccstr!("The variable that holds the input value."),
      STRING_VAR_SLICE,
    )
      .into(),
    (
      cstr!("Multiline"),
      shccstr!("Support multiple lines."),
      BOOL_TYPES_SLICE,
    )
      .into(),
  ];
}

impl Default for TextField {
  fn default() -> Self {
    let mut parents = ParamVar::default();
    parents.set_name(PARENTS_UI_NAME);
    Self {
      parents,
      requiring: Vec::new(),
      variable: ParamVar::default(),
      multiline: ParamVar::new(false.into()),
      exposing: Vec::new(),
      should_expose: false,
      mutable_text: true,
    }
  }
}

impl Shard for TextField {
  fn registerName() -> &'static str
  where
    Self: Sized,
  {
    cstr!("UI.TextField")
  }

  fn hash() -> u32
  where
    Self: Sized,
  {
    compile_time_crc32::crc32!("UI.TextField-rust-0x20200101")
  }

  fn name(&mut self) -> &str {
    "UI.TextField"
  }

  fn help(&mut self) -> OptionalString {
    OptionalString(shccstr!("A widget where text can be entered."))
  }

  fn inputTypes(&mut self) -> &Types {
    &NONE_TYPES
  }

  fn inputHelp(&mut self) -> OptionalString {
    *HELP_VALUE_IGNORED
  }

  fn outputTypes(&mut self) -> &Types {
    &STRING_TYPES
  }

  fn outputHelp(&mut self) -> OptionalString {
    OptionalString(shccstr!("The value produced when changed."))
  }

  fn parameters(&mut self) -> Option<&Parameters> {
    Some(&TEXTINPUT_PARAMETERS)
  }

  fn setParam(&mut self, index: i32, value: &Var) -> Result<(), &str> {
    match index {
      0 => Ok(self.variable.set_param(value)),
      1 => Ok(self.multiline.set_param(value)),
      _ => Err("Invalid parameter index"),
    }
  }

  fn getParam(&mut self, index: i32) -> Var {
    match index {
      0 => self.variable.get_param(),
      1 => self.multiline.get_param(),
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
          self.mutable_text = var.isMutable;
          if var.exposedType.basicType != shardsc::SHType_String {
            return Err("TextField: string variable required.");
          }
          break;
        }
      }
    } else {
      self.mutable_text = false;
    }

    Ok(common_type::string)
  }

  fn exposedVariables(&mut self) -> Option<&ExposedTypes> {
    if self.variable.is_variable() && self.should_expose {
      self.exposing.clear();

      let exp_info = ExposedInfo {
        exposedType: common_type::string,
        name: self.variable.get_name(),
        help: cstr!("The exposed string variable").into(),
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
    self.multiline.warmup(ctx);

    if self.should_expose {
      // new string
      let tmp = Var::ephemeral_string("");
      self.variable.set_cloning(&tmp);
    }

    Ok(())
  }

  fn cleanup(&mut self) -> Result<(), &str> {
    self.multiline.cleanup();
    self.variable.cleanup();
    self.parents.cleanup();

    Ok(())
  }

  fn activate(&mut self, _context: &Context, _input: &Var) -> Result<Var, &str> {
    if let Some(ui) = util::get_current_parent(self.parents.get())? {
      let mut mutable: MutVarTextBuffer;
      let mut immutable: VarTextBuffer;
      let text: &mut dyn egui::TextBuffer = if self.mutable_text {
        mutable = MutVarTextBuffer(self.variable.get_mut());
        &mut mutable
      } else {
        immutable = VarTextBuffer(self.variable.get());
        &mut immutable
      };
      let text_edit = if self.multiline.get().try_into()? {
        egui::TextEdit::multiline(text)
      } else {
        egui::TextEdit::singleline(text)
      };
      let response = ui.add(text_edit);

      if response.changed() || response.lost_focus() {
        Ok(*self.variable.get())
      } else {
        Ok(Var::default())
      }
    } else {
      Err("No UI parent")
    }
  }
}