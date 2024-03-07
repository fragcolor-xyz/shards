/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

use crate::util;
use crate::widgets::text_util;
use crate::ANY_TABLE_SLICE;
use crate::CONTEXTS_NAME;
use crate::PARENTS_UI_NAME;
use crate::STRING_VAR_SLICE;
use shards::core::register_shard;
use shards::shard::Shard;
use shards::types::common_type;
use shards::types::ClonedVar;
use shards::types::Context;
use shards::types::ExposedTypes;
use shards::types::InstanceData;
use shards::types::OptionalString;
use shards::types::ParamVar;
use shards::types::ShardsVar;
use shards::types::Type;
use shards::types::Types;
use shards::types::Var;
use shards::types::WireState;
use shards::types::ANY_TYPES;
use shards::types::BOOL_OR_NONE_SLICE;
use shards::types::BOOL_TYPES;
use shards::types::SHARDS_OR_NONE_TYPES;

#[derive(shard)]
#[shard_info("UI.Button", "Clickable button with text.")]
struct Button {
  #[shard_param("Label", "The text label of this button.", STRING_VAR_SLICE)]
  label: ParamVar,
  #[shard_param(
    "Action",
    "The shards to execute when the button is pressed.",
    SHARDS_OR_NONE_TYPES
  )]
  action: ShardsVar,
  #[shard_param("Style", "The text style.", ANY_TABLE_SLICE)]
  style: ClonedVar,
  #[shard_param("Wrap", "Wrap the text depending on the layout.", BOOL_OR_NONE_SLICE)]
  wrap: ClonedVar,
  #[shard_warmup]
  contexts: ParamVar,
  #[shard_warmup]
  parents: ParamVar,
  #[shard_required]
  required: ExposedTypes,
}

impl Default for Button {
  fn default() -> Self {
    Self {
      contexts: ParamVar::new_named(CONTEXTS_NAME),
      parents: ParamVar::new_named(PARENTS_UI_NAME),
      label: ParamVar::default(),
      action: ShardsVar::default(),
      wrap: ClonedVar::default(),
      style: ClonedVar::default(),
      required: Vec::new(),
    }
  }
}

#[shard_impl]
impl Shard for Button {
  fn input_types(&mut self) -> &Types {
    &ANY_TYPES
  }

  fn input_help(&mut self) -> OptionalString {
    OptionalString(shccstr!(
      "The value that will be passed to the Action shards of the button."
    ))
  }

  fn output_types(&mut self) -> &Types {
    &BOOL_TYPES
  }

  fn output_help(&mut self) -> OptionalString {
    OptionalString(shccstr!(
      "Indicates whether the button was clicked during this frame."
    ))
  }

  fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
    self.compose_helper(data)?;

    // Add UI.Parents to the list of required variables
    util::require_context(&mut self.required);
    util::require_parents(&mut self.required);

    self.action.compose(data)?;
    shards::util::require_shards_contents(&mut self.required, &self.action);

    Ok(common_type::bool)
  }

  fn warmup(&mut self, ctx: &Context) -> Result<(), &str> {
    self.warmup_helper(ctx)?;
    Ok(())
  }

  fn cleanup(&mut self, ctx: Option<&Context>) -> Result<(), &str> {
    self.cleanup_helper(ctx)?;
    Ok(())
  }

  fn activate(&mut self, context: &Context, input: &Var) -> Result<Var, &str> {
    if let Some(ui) = util::get_current_parent_opt(self.parents.get())? {
      let label: &str = self.label.get().try_into()?;
      let mut text = egui::RichText::new(label);

      let style = &self.style.0;
      if !style.is_none() {
        text = text_util::get_styled_text(text, &style.try_into()?)?;
      }

      let mut button = egui::Button::new(text);

      let wrap = &self.wrap.0;
      if !wrap.is_none() {
        let wrap: bool = wrap.try_into()?;
        button = button.wrap(wrap);
      }

      let mut button_clicked = false;

      let response = ui.add(button);
      if response.clicked() {
        let mut output = Var::default();
        if self.action.activate(context, input, &mut output) == WireState::Error {
          return Err("Failed to activate button");
        }

        // button clicked during this frame
        button_clicked = true;
      }

      // Store response in context to support shards like PopupWrapper, which uses a stored response in order to wrap behavior around it
      let ctx = util::get_current_context(&self.contexts)?;
      ctx.prev_response = Some(response);
      if button_clicked {
        ctx.override_selection_response = ctx.prev_response.clone();
      }

      // button not clicked during this frame
      Ok(button_clicked.into())
    } else {
      Err("No UI parent")
    }
  }
}

pub fn register_shards() {
  register_shard::<Button>();
}
