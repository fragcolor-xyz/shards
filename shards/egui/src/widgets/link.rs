/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

use super::Link;
use crate::util;
use crate::widgets::text_util;
use crate::ANY_TABLE_SLICE;
use crate::PARENTS_UI_NAME;
use shards::shard::LegacyShard;
use shards::types::common_type;
use shards::types::Context;
use shards::types::ExposedTypes;
use shards::types::InstanceData;
use shards::types::OptionalString;
use shards::types::ParamVar;
use shards::types::Parameters;
use shards::types::ShardsVar;
use shards::types::Type;
use shards::types::Types;
use shards::types::Var;
use shards::types::WireState;
use shards::types::ANY_TYPES;
use shards::types::BOOL_TYPES;
use shards::types::SHARDS_OR_NONE_TYPES;
use shards::types::STRING_OR_NONE_SLICE;

lazy_static! {
  static ref LINK_PARAMETERS: Parameters = vec![
    (
      cstr!("Label"),
      shccstr!("Optional label for the link."),
      STRING_OR_NONE_SLICE,
    )
      .into(),
    (
      cstr!("Action"),
      shccstr!("The shards to execute when the link is clicked."),
      &SHARDS_OR_NONE_TYPES[..],
    )
      .into(),
    (cstr!("Style"), shccstr!("The text style."), ANY_TABLE_SLICE,).into(),
  ];
}

impl Default for Link {
  fn default() -> Self {
    let mut parents = ParamVar::default();
    parents.set_name(PARENTS_UI_NAME);
    Self {
      parents,
      requiring: Vec::new(),
      label: ParamVar::default(),
      action: ShardsVar::default(),
      style: ParamVar::default(),
    }
  }
}

impl LegacyShard for Link {
  fn registerName() -> &'static str
  where
    Self: Sized,
  {
    cstr!("UI.Link")
  }

  fn hash() -> u32
  where
    Self: Sized,
  {
    compile_time_crc32::crc32!("UI.Link-rust-0x20200101")
  }

  fn name(&mut self) -> &str {
    "UI.Link"
  }

  fn help(&mut self) -> OptionalString {
    OptionalString(shccstr!("A clickable link."))
  }

  fn inputTypes(&mut self) -> &Types {
    &ANY_TYPES
  }

  fn inputHelp(&mut self) -> OptionalString {
    OptionalString(shccstr!(
      "The value that will be passed to the Action shards of the link."
    ))
  }

  fn outputTypes(&mut self) -> &Types {
    &BOOL_TYPES
  }

  fn outputHelp(&mut self) -> OptionalString {
    OptionalString(shccstr!(
      "Indicates whether the link was clicked during this frame."
    ))
  }

  fn parameters(&mut self) -> Option<&Parameters> {
    Some(&LINK_PARAMETERS)
  }

  fn setParam(&mut self, index: i32, value: &Var) -> Result<(), &str> {
    match index {
      0 => self.label.set_param(value),
      1 => self.action.set_param(value),
      2 => self.style.set_param(value),
      _ => Err("Invalid parameter index"),
    }
  }

  fn getParam(&mut self, index: i32) -> Var {
    match index {
      0 => self.label.get_param(),
      1 => self.action.get_param(),
      2 => self.style.get_param(),
      _ => Var::default(),
    }
  }

  fn requiredVariables(&mut self) -> Option<&ExposedTypes> {
    self.requiring.clear();

    // Add UI.Parents to the list of required variables
    util::require_parents(&mut self.requiring);

    Some(&self.requiring)
  }

  fn hasCompose() -> bool {
    true
  }

  fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
    if !self.action.is_empty() {
      self.action.compose(data)?;
    }

    Ok(common_type::bool)
  }

  fn warmup(&mut self, context: &Context) -> Result<(), &str> {
    self.parents.warmup(context);
    self.label.warmup(context);
    if !self.action.is_empty() {
      self.action.warmup(context)?;
    }
    self.style.warmup(context);

    Ok(())
  }

  fn cleanup(&mut self, ctx: Option<&Context>) -> Result<(), &str> {
    self.style.cleanup(ctx);
    if !self.action.is_empty() {
      self.action.cleanup(ctx);
    }
    self.label.cleanup(ctx);
    self.parents.cleanup(ctx);

    Ok(())
  }

  fn activate(&mut self, context: &Context, input: &Var) -> Result<Option<Var>, &str> {
    if let Some(ui) = util::get_current_parent_opt(self.parents.get())? {
      let label: &str = self.label.get().try_into()?;
      let mut text = egui::RichText::new(label);

      let style = self.style.get();
      if !style.is_none() {
        text = text_util::get_styled_text(text, &style.try_into()?)?;
      }

      let response = ui.add(egui::Link::new(text));
      if response.clicked() {
        let mut output = Var::default();
        if self.action.activate(context, input, &mut output) == WireState::Error {
          return Err("Failed to activate button");
        }

        // link clicked during this frame
        return Ok(Some(true.into()));
      }

      // link not clicked during this frame
      Ok(Some(false.into()))
    } else {
      Err("No UI parent")
    }
  }
}
