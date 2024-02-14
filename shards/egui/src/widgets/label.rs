/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

use super::Label;
use crate::util;
use crate::widgets::text_util;
use crate::ANY_TABLE_SLICE;
use crate::HELP_OUTPUT_EQUAL_INPUT;
use crate::PARENTS_UI_NAME;
use shards::shard::LegacyShard;
use shards::types::Context;
use shards::types::ExposedTypes;
use shards::types::OptionalString;
use shards::types::ParamVar;
use shards::types::Parameters;
use shards::types::Types;
use shards::types::Var;
use shards::types::BOOL_VAR_OR_NONE_SLICE;
use shards::types::STRING_TYPES;

lazy_static! {
  static ref LABEL_PARAMETERS: Parameters = vec![
    (
      cstr!("Wrap"),
      shccstr!("Wrap the text depending on the layout."),
      BOOL_VAR_OR_NONE_SLICE,
    )
      .into(),
    (cstr!("Style"), shccstr!("The text style."), ANY_TABLE_SLICE,).into(),
  ];
}

impl Default for Label {
  fn default() -> Self {
    let mut parents = ParamVar::default();
    parents.set_name(PARENTS_UI_NAME);
    Self {
      parents,
      requiring: Vec::new(),
      wrap: ParamVar::default(),
      style: ParamVar::default(),
    }
  }
}

impl LegacyShard for Label {
  fn registerName() -> &'static str
  where
    Self: Sized,
  {
    cstr!("UI.Label")
  }

  fn hash() -> u32
  where
    Self: Sized,
  {
    compile_time_crc32::crc32!("UI.Label-rust-0x20200101")
  }

  fn name(&mut self) -> &str {
    "UI.Label"
  }

  fn help(&mut self) -> OptionalString {
    OptionalString(shccstr!("Static text."))
  }

  fn inputTypes(&mut self) -> &Types {
    &STRING_TYPES
  }

  fn inputHelp(&mut self) -> OptionalString {
    OptionalString(shccstr!("The text to display."))
  }

  fn outputTypes(&mut self) -> &Types {
    &STRING_TYPES
  }

  fn outputHelp(&mut self) -> OptionalString {
    *HELP_OUTPUT_EQUAL_INPUT
  }

  fn parameters(&mut self) -> Option<&Parameters> {
    Some(&LABEL_PARAMETERS)
  }

  fn setParam(&mut self, index: i32, value: &Var) -> Result<(), &str> {
    match index {
      0 => self.wrap.set_param(value),
      1 => self.style.set_param(value),
      _ => Err("Invalid parameter index"),
    }
  }

  fn getParam(&mut self, index: i32) -> Var {
    match index {
      0 => self.wrap.get_param(),
      1 => self.style.get_param(),
      _ => Var::default(),
    }
  }

  fn requiredVariables(&mut self) -> Option<&ExposedTypes> {
    self.requiring.clear();

    // Add UI.Parents to the list of required variables
    util::require_parents(&mut self.requiring);

    Some(&self.requiring)
  }

  fn warmup(&mut self, context: &Context) -> Result<(), &str> {
    self.parents.warmup(context);

    self.wrap.warmup(context);
    self.style.warmup(context);

    Ok(())
  }

  fn cleanup(&mut self, ctx: Option<&Context>) -> Result<(), &str> {
    self.parents.cleanup(ctx);

    self.style.cleanup(ctx);
    self.wrap.cleanup(ctx);

    Ok(())
  }

  fn activate(&mut self, _context: &Context, input: &Var) -> Result<Var, &str> {
    if let Some(ui) = util::get_current_parent_opt(self.parents.get())? {
      let text: &str = input.try_into()?;
      let mut text = egui::RichText::new(text); // allocation here, todo cache

      let style = self.style.get();
      if !style.is_none() {
        text = text_util::get_styled_text(text, &style.try_into()?)?;
      }

      let mut label = egui::Label::new(text);

      let wrap = self.wrap.get();
      if !wrap.is_none() {
        let wrap: bool = wrap.try_into()?;
        label = label.wrap(wrap);
      }

      ui.add(label);

      Ok(*input)
    } else {
      Err("No UI parent")
    }
  }
}
