/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

use crate::util;
use crate::MutVarTextBuffer;
use crate::VarTextBuffer;
use crate::HELP_VALUE_IGNORED;
use crate::PARENTS_UI_NAME;
use crate::STRING_VAR_SLICE;
use crate::FLOAT_VAR_OR_NONE_SLICE;
use egui::RichText;
use shards::shard::Shard;
use shards::shardsc;
use shards::types::common_type;
use shards::types::ClonedVar;
use shards::types::Context;
use shards::types::ExposedInfo;
use shards::types::ExposedTypes;
use shards::types::InstanceData;
use shards::types::OptionalString;
use shards::types::ParamVar;
use shards::types::Type;
use shards::types::Types;
use shards::types::Var;
use shards::types::ANYS_TYPES;
use shards::types::BOOL_TYPES_SLICE;
use shards::types::NONE_TYPES;
use std::cmp::Ordering;
use std::ffi::CStr;

lazy_static! {
  static ref TEXTINPUT_OUTPUT_TYPES: Types = vec![common_type::any];
}

#[derive(shards::shard)]
#[shard_info("UI.TextField", "A widget where text can be entered.")]
pub struct TextField {
  #[shard_param(
    "Variable",
    "The variable that holds the input value.",
    STRING_VAR_SLICE
  )]
  variable: ParamVar,
  #[shard_param(
    "JustifyWidth",
    "Whether to take up all available space for its desired width. Takes priority over Desired Width.",
    BOOL_TYPES_SLICE
  )]
  justify_width: ClonedVar,
  #[shard_param(
    "DesiredWidth",
    "The desired width of the text field.",
    FLOAT_VAR_OR_NONE_SLICE
  )]
  desired_width: ParamVar,
  #[shard_param(
    "ClipText",
    "Whether to clip the text if it exceeds the width of the text field. Or expand the text field to fit the text.",
    BOOL_TYPES_SLICE
  )]
  clip_text: ClonedVar,
  #[shard_param("Multiline", "Support multiple lines.", BOOL_TYPES_SLICE)]
  multiline: ClonedVar,
  #[shard_param("Password", "Support multiple lines.", BOOL_TYPES_SLICE)]
  password: ClonedVar,
  #[shard_param("Hint", "Hint to show in the text field.", [common_type::string, common_type::string_var, common_type::none])]
  hint: ParamVar,
  #[shard_warmup]
  parents: ParamVar,
  #[shard_required]
  requiring: ExposedTypes,
  exposing: ExposedTypes,
  should_expose: bool,
  mutable_text: bool,
}

impl Default for TextField {
  fn default() -> Self {
    Self {
      parents: ParamVar::new_named(PARENTS_UI_NAME),
      requiring: Vec::new(),
      variable: ParamVar::default(),
      justify_width: false.into(),
      desired_width: ParamVar::default(),
      clip_text: true.into(),
      multiline: false.into(),
      password: false.into(),
      hint: ParamVar::default(),
      exposing: Vec::new(),
      should_expose: false,
      mutable_text: true,
    }
  }
}

#[shards::shard_impl]
impl Shard for TextField {
  fn input_types(&mut self) -> &Types {
    &NONE_TYPES
  }

  fn input_help(&mut self) -> OptionalString {
    *HELP_VALUE_IGNORED
  }

  fn output_types(&mut self) -> &Types {
    &ANYS_TYPES
  }

  fn output_help(&mut self) -> OptionalString {
    OptionalString(shccstr!("The value produced when changed."))
  }

  fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
    self.compose_helper(data)?;

    // Add UI.Parents to the list of required variables
    util::require_parents(&mut self.requiring);

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

    self.exposing.clear();
    if self.variable.is_variable() && self.should_expose {
      let exp_info = ExposedInfo {
        exposedType: common_type::string,
        name: self.variable.get_name(),
        help: shccstr!("The exposed string variable"),
        ..ExposedInfo::default()
      };

      self.exposing.push(exp_info);
    }

    Ok(common_type::any)
  }

  fn exposed_variables(&mut self) -> Option<&ExposedTypes> {
    Some(&self.exposing)
  }

  fn warmup(&mut self, ctx: &Context) -> Result<(), &str> {
    self.warmup_helper(ctx)?;

    if self.should_expose {
      // new string
      let tmp = Var::ephemeral_string("");
      self.variable.set_cloning(&tmp);
    }

    Ok(())
  }

  fn cleanup(&mut self, ctx: Option<&Context>) -> Result<(), &str> {
    self.cleanup_helper(ctx)?;

    Ok(())
  }

  fn activate(&mut self, _context: &Context, _input: &Var) -> Result<Var, &str> {
    let ui = util::get_current_parent_opt(self.parents.get())?.ok_or("No parent UI")?;

    let mut mutable: MutVarTextBuffer;
    let mut immutable: VarTextBuffer;
    let text: &mut dyn egui::TextBuffer = if self.mutable_text {
      mutable = MutVarTextBuffer(self.variable.get_mut());
      &mut mutable
    } else {
      immutable = VarTextBuffer(self.variable.get());
      &mut immutable
    };

    let multi_line: bool = (&self.multiline.0).try_into().unwrap_or(false);
    let mut text_edit = if multi_line {
      egui::TextEdit::multiline(text)
    } else {
      egui::TextEdit::singleline(text)
    };

    if TryInto::<bool>::try_into(&self.password.0).unwrap_or(false) {
      text_edit = text_edit.password(true);
    }

    if let Ok(hint) = TryInto::<&str>::try_into(self.hint.get()) {
      text_edit = text_edit.hint_text(RichText::from(hint).color(egui::Color32::GRAY).italics());
    }

    let justify_width: bool = (&self.justify_width.0).try_into().unwrap(); // qed, shards validation
    text_edit = if justify_width {
      text_edit.desired_width(f32::INFINITY)
    } else {
      if let Ok(desired_width) = TryInto::<f64>::try_into(self.desired_width.get()) {
        text_edit.desired_width(desired_width as f32)
      } else {
        text_edit
      }
    };

    let clip_text: bool = (&self.clip_text.0).try_into().unwrap(); // qed, shards validation
    text_edit = text_edit.clip_text(clip_text);

    let response = ui.add(text_edit);

    if response.changed() {
      Ok(*self.variable.get())
    } else {
      Ok(Var::default())
    }
  }
}
