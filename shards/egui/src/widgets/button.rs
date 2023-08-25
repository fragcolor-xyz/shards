/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

use super::Button;
use crate::util;
use crate::widgets::text_util;
use crate::ANY_TABLE_SLICE;
use crate::PARENTS_UI_NAME;
use shards::shard::Shard;
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
use shards::types::BOOL_OR_NONE_SLICE;
use shards::types::BOOL_TYPES;
use shards::types::SHARDS_OR_NONE_TYPES;
use shards::types::STRING_TYPES;

lazy_static! {
  static ref BUTTON_PARAMETERS: Parameters = vec![
    (
      cstr!("Label"),
      shccstr!("The text label of this button."),
      &STRING_TYPES[..],
    )
      .into(),
    (
      cstr!("Action"),
      shccstr!("The shards to execute when the button is pressed."),
      &SHARDS_OR_NONE_TYPES[..],
    )
      .into(),
    (
      cstr!("Wrap"),
      shccstr!("Wrap the text depending on the layout."),
      BOOL_OR_NONE_SLICE,
    )
      .into(),
    (cstr!("Style"), shccstr!("The text style."), ANY_TABLE_SLICE,).into(),
  ];
}

impl Default for Button {
  fn default() -> Self {
    let mut parents = ParamVar::default();
    parents.set_name(PARENTS_UI_NAME);
    Self {
      parents,
      requiring: Vec::new(),
      label: ParamVar::default(),
      action: ShardsVar::default(),
      wrap: ParamVar::default(),
      style: ParamVar::default(),
    }
  }
}

impl Shard for Button {
  fn registerName() -> &'static str
  where
    Self: Sized,
  {
    cstr!("UI.Button")
  }

  fn hash() -> u32
  where
    Self: Sized,
  {
    compile_time_crc32::crc32!("UI.Button-rust-0x20200101")
  }

  fn name(&mut self) -> &str {
    "UI.Button"
  }

  fn help(&mut self) -> OptionalString {
    OptionalString(shccstr!("Clickable button with text."))
  }

  fn inputTypes(&mut self) -> &Types {
    &ANY_TYPES
  }

  fn inputHelp(&mut self) -> OptionalString {
    OptionalString(shccstr!(
      "The value that will be passed to the Action shards of the button."
    ))
  }

  fn outputTypes(&mut self) -> &Types {
    &BOOL_TYPES
  }

  fn outputHelp(&mut self) -> OptionalString {
    OptionalString(shccstr!(
      "Indicates whether the button was clicked during this frame."
    ))
  }

  fn parameters(&mut self) -> Option<&Parameters> {
    Some(&BUTTON_PARAMETERS)
  }

  fn setParam(&mut self, index: i32, value: &Var) -> Result<(), &str> {
    match index {
      0 => self.label.set_param(value),
      1 => self.action.set_param(value),
      2 => self.wrap.set_param(value),
      3 => self.style.set_param(value),
      _ => Err("Invalid parameter index"),
    }
  }

  fn getParam(&mut self, index: i32) -> Var {
    match index {
      0 => self.label.get_param(),
      1 => self.action.get_param(),
      2 => self.wrap.get_param(),
      3 => self.style.get_param(),
      _ => Var::default(),
    }
  }

  fn requiredVariables(&mut self) -> Option<&ExposedTypes> {
    self.requiring.clear();

    // Add UI.Parents to the list of required variables
    util::require_parents(&mut self.requiring, &self.parents);

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

  fn warmup(&mut self, ctx: &Context) -> Result<(), &str> {
    self.parents.warmup(ctx);

    self.label.warmup(ctx);
    if !self.action.is_empty() {
      self.action.warmup(ctx)?;
    }
    self.wrap.warmup(ctx);
    self.style.warmup(ctx);

    Ok(())
  }

  fn cleanup(&mut self) -> Result<(), &str> {
    self.style.cleanup();
    self.wrap.cleanup();
    if !self.action.is_empty() {
      self.action.cleanup();
    }
    self.label.cleanup();

    self.parents.cleanup();

    Ok(())
  }

  fn activate(&mut self, context: &Context, input: &Var) -> Result<Var, &str> {
    if let Some(ui) = util::get_current_parent(self.parents.get())? {
      let label: &str = self.label.get().try_into()?;
      let mut text = egui::RichText::new(label);

      let style = self.style.get();
      if !style.is_none() {
        text = text_util::get_styled_text(text, &style.try_into()?)?;
      }

      let mut button = egui::Button::new(text);

      let wrap = self.wrap.get();
      if !wrap.is_none() {
        let wrap: bool = wrap.try_into()?;
        button = button.wrap(wrap);
      }

      let response = ui.add(button);
      if response.clicked() {
        let mut output = Var::default();
        if self.action.activate(context, input, &mut output) == WireState::Error {
          return Err("Failed to activate button");
        }

        // button clicked during this frame
        return Ok(true.into());
      }

      // button not clicked during this frame
      Ok(false.into())
    } else {
      Err("No UI parent")
    }
  }
}
