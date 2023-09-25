/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

use crate::util;
use crate::widgets::text_util;
use crate::HELP_OUTPUT_EQUAL_INPUT;
use super::PopupLocation;
use super::POPUPLOCATION_TYPES;
use shards::cstr;
use shards::shard;
use shards::shard::Shard;
use shards::shard_impl;
use shards::types::FLOAT_OR_NONE_TYPES_SLICE;
use shards::types::OptionalString;
use shards::types::ANY_TYPES;
use shards::types::{
  common_type, Context, ExposedInfo, ExposedTypes, InstanceData, ParamVar, ShardsVar, Type,
  Types, Var, ANYS_TYPES, SHARDS_OR_NONE_TYPES, STRING_TYPES,
};

use crate::{CONTEXTS_NAME, PARENTS_UI_NAME};
use shards::{
  core::register_shard,
  types::{BOOL_OR_NONE_SLICE, STRING_VAR_OR_NONE_SLICE},
};



#[derive(shard)]
#[shard_info(
  "UI.PopupButton",
  "Wraps a button with a popup that can act as a drop-down menu or suggestion menu."
)]
struct PopupButtonShard {
  #[shard_param("Label", "The text label of the button.", STRING_TYPES)]
  pub label: ParamVar,
  #[shard_param("Wrap", "Wrap the text depending on the layout.", BOOL_OR_NONE_SLICE)]
  pub wrap: ParamVar,
  #[shard_param("Style", "The text style.", ANYS_TYPES)]
  pub style: ParamVar,
  #[shard_param("MinWidth", "The minimum width of the popup that should appear below or above the button. By default, it is always at least as wide as the button.", FLOAT_OR_NONE_TYPES_SLICE)]
  pub min_width: ParamVar,
  #[shard_param("AboveOrBelow", "Whether the location of the popup should be above or below the button.", POPUPLOCATION_TYPES)]
  pub above_or_below: ParamVar,
  #[shard_param("ID", "An optional ID value to make the popup unique if the label text collides.", STRING_VAR_OR_NONE_SLICE)]
  pub id: ParamVar,
  pub cached_id: Option<egui::Id>,
  #[shard_param(
    "Contents",
    "The shards to execute and render inside the popup ui when the button is pressed.",
    SHARDS_OR_NONE_TYPES
  )]
  pub contents: ShardsVar,
  #[shard_warmup]
  contexts: ParamVar,
  #[shard_warmup]
  parents: ParamVar,
  #[shard_required]
  required: ExposedTypes,
}

impl Default for PopupButtonShard {
  fn default() -> Self {
    Self {
      contexts: ParamVar::new_named(CONTEXTS_NAME),
      parents: ParamVar::new_named(PARENTS_UI_NAME),
      label: ParamVar::default(),
      wrap: ParamVar::default(),
      style: ParamVar::default(),
      above_or_below: ParamVar::default(),
      min_width: ParamVar::default(),
      id: ParamVar::default(),
      cached_id: None,
      contents: ShardsVar::default(),
      required: Vec::new(),
    }
  }
}

#[shard_impl]
impl Shard for PopupButtonShard {
  fn input_types(&mut self) -> &Types {
    &ANY_TYPES
  }

  fn input_help(&mut self) -> OptionalString {
    OptionalString(shccstr!(
      "The value that will be passed to the Contents shards of the rendered window."
    ))
  }

  fn output_types(&mut self) -> &Types {
    &ANY_TYPES
  }

  fn output_help(&mut self) -> OptionalString {
    *HELP_OUTPUT_EQUAL_INPUT
  }

  fn warmup(&mut self, context: &Context) -> Result<(), &str> {
    self.warmup_helper(context)?;

    Ok(())
  }

  fn cleanup(&mut self) -> Result<(), &str> {
    self.cleanup_helper()?;

    Ok(())
  }

  fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
    self.compose_helper(data)?;

    util::require_context(&mut self.required);
    util::require_parents(&mut self.required);

    if self.id.is_variable() {
      let id_info = ExposedInfo {
        exposedType: common_type::string,
        name: self.id.get_name(),
        help: cstr!("The ID variable.").into(),
        ..ExposedInfo::default()
      };
      self.required.push(id_info);
    }

    if !self.contents.is_empty() {
      self.contents.compose(data)?;
    }

    // Always passthrough the input
    Ok(data.inputType)
  }

  fn activate(&mut self, context: &Context, input: &Var) -> Result<Var, &str> {
    if let Some(ui) = util::get_current_parent_opt(self.parents.get())? {
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
      let popup_id = if let Ok(id) = <&str>::try_from(self.id.get()) {
        self.cached_id.get_or_insert_with(|| ui.make_persistent_id(id))
      } else {
        self.cached_id.get_or_insert_with(|| ui.make_persistent_id(label))
      };

      if response.clicked() {
        ui.memory_mut(|mem| mem.toggle_popup(*popup_id));
      }

      let above_or_below = if self.above_or_below.get().is_none() {
        egui::AboveOrBelow::Below
      } else {
        let above_or_below: PopupLocation = self.above_or_below.get().try_into()?;
        above_or_below.into()
      };
      
      if !self.contents.is_empty() {
        if let Some(inner) = egui::popup::popup_above_or_below_widget(ui, *popup_id, &response, above_or_below, |ui| {
          if !self.min_width.get().is_none() {
            ui.set_min_width(self.min_width.get().try_into()?);
          }
          util::activate_ui_contents(context, input, ui, &mut self.parents, &mut self.contents)
        }) {
          // Only if popup is open will there be an inner result. In such a case, verify that nothing went wrong.
          inner?;
        }
      }
    } else {
      return Err("No UI parent");
    }

    // Always passthrough the input
    Ok(*input)
  }
}

pub fn register_shards() {
  register_shard::<PopupButtonShard>();
}
