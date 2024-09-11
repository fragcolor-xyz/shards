/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

use super::Tooltip;
use crate::util;
use crate::HELP_OUTPUT_EQUAL_INPUT;
use crate::PARENTS_UI_NAME;
use crate::STRING_OR_SHARDS_OR_NONE_TYPES_SLICE;
use shards::shard::LegacyShard;
use shards::types::common_type;
use shards::types::Context;

use shards::types::ExposedInfo;
use shards::types::ExposedTypes;
use shards::types::InstanceData;
use shards::types::OptionalString;
use shards::types::ParamVar;
use shards::types::Parameters;
use shards::types::ShardsVar;
use shards::types::Type;
use shards::types::Types;
use shards::types::Var;
use shards::types::ANY_TYPES;
use shards::types::SHARDS_OR_NONE_TYPES;

lazy_static! {
  static ref TOOLTIP_PARAMETERS: Parameters = vec![
    (
      cstr!("Contents"),
      shccstr!("The UI contents."),
      &SHARDS_OR_NONE_TYPES[..],
    )
      .into(),
    (
      cstr!("OnHover"),
      shccstr!("The tooltip contents."),
      STRING_OR_SHARDS_OR_NONE_TYPES_SLICE,
    )
      .into(),
  ];
}

impl Default for Tooltip {
  fn default() -> Self {
    let mut parents = ParamVar::default();
    parents.set_name(PARENTS_UI_NAME);
    Self {
      parents,
      requiring: Vec::new(),
      contents: ShardsVar::default(),
      text: ParamVar::default(),
      onhover: ShardsVar::default(),
      exposing: Vec::new(),
    }
  }
}

impl LegacyShard for Tooltip {
  fn registerName() -> &'static str
  where
    Self: Sized,
  {
    cstr!("UI.Tooltip")
  }

  fn hash() -> u32
  where
    Self: Sized,
  {
    compile_time_crc32::crc32!("UI.Tooltip-rust-0x20200101")
  }

  fn name(&mut self) -> &str {
    "UI.Tooltip"
  }

  fn help(&mut self) -> OptionalString {
    OptionalString(shccstr!(
      "Display a tooltip when the Contents is hovered over."
    ))
  }

  fn inputTypes(&mut self) -> &Types {
    &ANY_TYPES
  }

  fn inputHelp(&mut self) -> OptionalString {
    OptionalString(shccstr!(
      "The value that will be passed to both the Contents and OnHover shards of the tooltip."
    ))
  }

  fn outputTypes(&mut self) -> &Types {
    &ANY_TYPES
  }

  fn outputHelp(&mut self) -> OptionalString {
    *HELP_OUTPUT_EQUAL_INPUT
  }

  fn parameters(&mut self) -> Option<&Parameters> {
    Some(&TOOLTIP_PARAMETERS)
  }

  fn setParam(&mut self, index: i32, value: &Var) -> Result<(), &str> {
    match index {
      0 => self.contents.set_param(value),
      1 if value.is_none() || value.is_string() || value.is_context_var() => {
        self.text.set_param(value)
      }
      1 => self.onhover.set_param(value),
      _ => Err("Invalid parameter index"),
    }
  }

  fn getParam(&mut self, index: i32) -> Var {
    match index {
      0 => self.contents.get_param(),
      1 if self.onhover.is_empty() => self.text.get_param(),
      1 => self.onhover.get_param(),
      _ => Var::default(),
    }
  }

  fn requiredVariables(&mut self) -> Option<&ExposedTypes> {
    self.requiring.clear();

    // Add UI.Parents to the list of required variables
    util::require_parents(&mut self.requiring);

    if self.text.is_variable() {
      let exp_info = ExposedInfo {
        exposedType: common_type::string,
        name: self.text.get_name(),
        help: shccstr!("The heading text or widgets for this collapsing header."),
        ..ExposedInfo::default()
      };
      self.requiring.push(exp_info);
    }

    Some(&self.requiring)
  }

  fn exposedVariables(&mut self) -> Option<&ExposedTypes> {
    self.exposing.clear();

    if util::expose_contents_variables(&mut self.exposing, &self.contents) {
      Some(&self.exposing)
    } else {
      None
    }
  }

  fn hasCompose() -> bool {
    true
  }

  fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
    if !self.contents.is_empty() {
      self.contents.compose(data)?;
    }
    if !self.onhover.is_empty() {
      self.onhover.compose(data)?;
    }

    // Always passthrough the input
    Ok(data.inputType)
  }

  fn warmup(&mut self, ctx: &Context) -> Result<(), &str> {
    self.parents.warmup(ctx);

    if !self.contents.is_empty() {
      self.contents.warmup(ctx)?;
    }
    self.text.warmup(ctx);
    if !self.onhover.is_empty() {
      self.onhover.warmup(ctx)?;
    }

    Ok(())
  }

  fn cleanup(&mut self, ctx: Option<&Context>) -> Result<(), &str> {
    if !self.onhover.is_empty() {
      self.onhover.cleanup(ctx);
    }
    self.text.cleanup(ctx);
    if !self.contents.is_empty() {
      self.contents.cleanup(ctx);
    }

    self.parents.cleanup(ctx);

    Ok(())
  }

  fn activate(&mut self, context: &Context, input: &Var) -> Result<Option<Var>, &str> {
    if self.contents.is_empty() {
      return Ok(None);
    }

    if let Some(ui) = util::get_current_parent_opt(self.parents.get())? {
      let response = {
        let inner_response = ui.scope(|ui| {
          util::activate_ui_contents(context, input, ui, &mut self.parents, &mut self.contents)
        });
        inner_response.inner?;
        inner_response.response
      };

      if response.hovered() {
        if self.onhover.is_empty() {
          let text: &str = self.text.get().try_into()?;
          response.on_hover_text(text);
        } else {
          let mut ret = Ok(Var::default());
          response.on_hover_ui(|ui| {
            ret =
              util::activate_ui_contents(context, input, ui, &mut self.parents, &mut self.onhover);
          });

          ret?;
        }
      }

      Ok(None)
    } else {
      Err("No UI parent")
    }
  }
}
