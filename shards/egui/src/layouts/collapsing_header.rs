/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

use super::CollapsingHeader;
use crate::util;
use crate::util::with_possible_panic;
use crate::EguiId;
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
use shards::types::BOOL_OR_VAR_SLICE;
use shards::types::SHARDS_OR_NONE_TYPES;

lazy_static! {
  static ref COLLAPSING_PARAMETERS: Parameters = vec![
    (
      cstr!("Heading"),
      shccstr!("The heading text or widgets for this collapsing header."),
      STRING_OR_SHARDS_OR_NONE_TYPES_SLICE,
    )
      .into(),
    (
      cstr!("Contents"),
      shccstr!("The UI contents."),
      &SHARDS_OR_NONE_TYPES[..],
    )
      .into(),
    (
      cstr!("DefaultOpen"),
      shccstr!("Whether the collapsing header is opened by default."),
      BOOL_OR_VAR_SLICE,
    )
      .into(),
  ];
}

impl Default for CollapsingHeader {
  fn default() -> Self {
    let mut parents = ParamVar::default();
    parents.set_name(PARENTS_UI_NAME);
    Self {
      parents,
      requiring: Vec::new(),
      text: ParamVar::default(),
      header: ShardsVar::default(),
      contents: ShardsVar::default(),
      default_open: ParamVar::new(false.into()),
      exposing: Vec::new(),
    }
  }
}

impl LegacyShard for CollapsingHeader {
  fn registerName() -> &'static str
  where
    Self: Sized,
  {
    cstr!("UI.Collapsing")
  }

  fn hash() -> u32
  where
    Self: Sized,
  {
    compile_time_crc32::crc32!("UI.Collapsing-rust-0x20200101")
  }

  fn name(&mut self) -> &str {
    "UI.Collapsing"
  }

  fn help(&mut self) -> OptionalString {
    OptionalString(shccstr!(
      "A header which can be collapsed/expanded, revealing a contained UI region."
    ))
  }

  fn inputTypes(&mut self) -> &Types {
    &ANY_TYPES
  }

  fn inputHelp(&mut self) -> OptionalString {
    OptionalString(shccstr!(
      "The value that will be passed to the Contents shards of the collapsing header."
    ))
  }

  fn outputTypes(&mut self) -> &Types {
    &ANY_TYPES
  }

  fn outputHelp(&mut self) -> OptionalString {
    *HELP_OUTPUT_EQUAL_INPUT
  }

  fn parameters(&mut self) -> Option<&Parameters> {
    Some(&COLLAPSING_PARAMETERS)
  }

  fn setParam(&mut self, index: i32, value: &Var) -> Result<(), &str> {
    match index {
      0 if value.is_none() || value.is_string() || value.is_context_var() => {
        self.text.set_param(value)
      }
      0 => self.header.set_param(value),
      1 => self.contents.set_param(value),
      2 => self.default_open.set_param(value),
      _ => Err("Invalid parameter index"),
    }
  }

  fn getParam(&mut self, index: i32) -> Var {
    match index {
      0 if self.header.is_empty() => self.text.get_param(),
      0 => self.header.get_param(),
      1 => self.contents.get_param(),
      2 => self.default_open.get_param(),
      _ => Var::default(),
    }
  }

  fn requiredVariables(&mut self) -> Option<&ExposedTypes> {
    self.requiring.clear();

    // Add UI.Parents to the list of required variables
    util::require_parents(&mut self.requiring);

    if self.default_open.is_variable() {
      let exp_info = ExposedInfo {
        exposedType: common_type::bool,
        name: self.default_open.get_name(),
        help: shccstr!("Whether the collapsing header is opened by default."),
        ..ExposedInfo::default()
      };
      self.requiring.push(exp_info);
    }

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

    let mut exposed = false;
    exposed |= util::expose_contents_variables(&mut self.exposing, &self.header);

    if exposed {
      Some(&self.exposing)
    } else {
      None
    }
  }

  fn hasCompose() -> bool {
    true
  }

  fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
    if !self.header.is_empty() {
      self.header.compose(data)?;
    }

    if !self.contents.is_empty() {
      self.contents.compose(data)?;
    }

    // Always passthrough the input
    Ok(data.inputType)
  }

  fn warmup(&mut self, ctx: &Context) -> Result<(), &str> {
    self.parents.warmup(ctx);

    self.text.warmup(ctx);

    if !self.header.is_empty() {
      self.header.warmup(ctx)?;
    }

    if !self.contents.is_empty() {
      self.contents.warmup(ctx)?;
    }

    self.default_open.warmup(ctx);

    Ok(())
  }

  fn cleanup(&mut self, ctx: Option<&Context>) -> Result<(), &str> {
    self.default_open.cleanup(ctx);

    if !self.contents.is_empty() {
      self.contents.cleanup(ctx);
    }

    if !self.header.is_empty() {
      self.header.cleanup(ctx);
    }
    self.text.cleanup(ctx);

    self.parents.cleanup(ctx);

    Ok(())
  }

  fn activate(&mut self, context: &Context, input: &Var) -> Result<Option<Var>, &str> {
    with_possible_panic(|| {
      if let Some(ui) = util::get_current_parent_opt(self.parents.get())? {
        let default_open: bool = self.default_open.get().try_into()?;

        // yes the following is unused if not an heading, but the API so so convoluted that this is the best solution for clarity sake
        let state = egui::collapsing_header::CollapsingState::load_with_default_open(
          ui.ctx(),
          egui::Id::new(EguiId::new(self, 0)),
          default_open,
        );

        if let Some(ret) = if self.header.is_empty() {
          let text: &str = self.text.get().try_into()?;
          egui::CollapsingHeader::new(text)
            .id_source(EguiId::new(self, 0))
            .default_open(default_open)
            .show(ui, |ui| {
              util::activate_ui_contents(context, input, ui, &mut self.parents, &mut self.contents)
            })
            .body_returned
        } else if let Some(body_response) = state
          .show_header(ui, |ui| {
            util::activate_ui_contents(context, input, ui, &mut self.parents, &mut self.header)
          })
          .body(|ui| {
            util::activate_ui_contents(context, input, ui, &mut self.parents, &mut self.contents)
          })
          .2
        {
          Some(body_response.inner)
        } else {
          None
        } {
          match ret {
            Err(err) => Err(err),
            Ok(_) => Ok(None),
          }
        } else {
          Ok(None)
        }
      } else {
        Err("No UI parent")
      }
    })?
  }
}
