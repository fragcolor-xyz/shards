/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

use super::PopupLocation;
use super::POPUPLOCATION_TYPES;
use crate::util;
use crate::EguiId;
use crate::HELP_OUTPUT_EQUAL_INPUT;
use shards::cstr;
use shards::shard;
use shards::shard::Shard;
use shards::shard_impl;
use shards::types::OptionalString;
use shards::types::ANY_TYPES;
use shards::types::FLOAT_OR_NONE_TYPES_SLICE;
use shards::types::{
  common_type, Context, ExposedInfo, ExposedTypes, InstanceData, ParamVar, ShardsVar, Type, Types,
  Var, SHARDS_OR_NONE_TYPES,
};

use crate::{CONTEXTS_NAME, PARENTS_UI_NAME};
use shards::{core::register_shard, types::STRING_VAR_OR_NONE_SLICE};

#[derive(shard)]
#[shard_info(
  "UI.PopupWrapper",
  "Wraps a button with a popup that can act as a drop-down menu or suggestion menu."
)]
struct PopupWrapper {
  #[shard_param("MinWidth", "The minimum width of the popup that should appear below or above the button. By default, it is always at least as wide as the button.", FLOAT_OR_NONE_TYPES_SLICE)]
  pub min_width: ParamVar,
  #[shard_param(
    "AboveOrBelow",
    "Whether the location of the popup should be above or below the button.",
    POPUPLOCATION_TYPES
  )]
  pub above_or_below: ParamVar,
  #[shard_param(
    "ID",
    "An optional ID value to make the popup unique if the label text collides.",
    STRING_VAR_OR_NONE_SLICE
  )]
  pub id: ParamVar,
  pub cached_id: Option<egui::Id>,
  #[shard_param(
    "Widget",
    "The shard(s) to execute that should contain a widget that supports having this popup generated for it upon being clicked.",
    SHARDS_OR_NONE_TYPES
  )]
  pub widget: ShardsVar,
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

impl Default for PopupWrapper {
  fn default() -> Self {
    Self {
      contexts: ParamVar::new_named(CONTEXTS_NAME),
      parents: ParamVar::new_named(PARENTS_UI_NAME),
      required: Vec::new(),
      above_or_below: ParamVar::default(),
      min_width: ParamVar::default(),
      id: ParamVar::default(),
      cached_id: None,
      widget: ShardsVar::default(),
      contents: ShardsVar::default(),
    }
  }
}

#[shard_impl]
impl Shard for PopupWrapper {
  fn input_types(&mut self) -> &Types {
    &ANY_TYPES
  }

  fn input_help(&mut self) -> OptionalString {
    OptionalString(shccstr!(
      "The value that will be passed to the Widget shard(s) of the popup."
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

  fn cleanup(&mut self, ctx: Option<&Context>) -> Result<(), &str> {
    self.cleanup_helper(ctx)?;

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
        help: shccstr!("The ID variable."),
        ..ExposedInfo::default()
      };
      self.required.push(id_info);
    }

    // TODO: Double check this double compose
    let mut data = *data;

    let output_type = self.widget.compose(&data)?.outputType;
    data.inputType = output_type;
    shards::util::require_shards_contents(&mut self.required, &self.widget);

    self.contents.compose(&data)?;
    shards::util::require_shards_contents(&mut self.required, &self.contents);

    // Always passthrough the input
    Ok(data.inputType)
  }

  fn activate(&mut self, context: &Context, input: &Var) -> Result<Var, &str> {
    if let Some(ui) = util::get_current_parent_opt(self.parents.get())? {
      let widget_result = if !self.widget.is_empty() {
        // Activate the widget that should leave a response inside the context.
        util::activate_ui_contents(context, input, ui, &mut self.parents, &mut self.widget)?
      } else {
        return Err("No widget provided for PopupWrapper.");
      };

      if !self.contents.is_empty() {
        // Retrieve previous response from context (that should be added by ui contents like buttons)
        let ctx = util::get_current_context(&self.contexts)?;
        let response = if let Some(response) = ctx.prev_response.take() {
          response
        } else {
          // Do nothing and passthrough input since there is no supported widget that has given a response.
          return Ok(*input);
        };

        // If provided a custom id, we do not cache it because a new id may be provided to the same shard multiple times lest ID conflicts
        let mut custom_popup_id = if !self.id.get().is_none() {
          let id: &str = self.id.get().try_into()?;
          Some(ui.make_persistent_id(id))
        } else {
          None
        };

        let popup_id = if !self.id.get().is_none() {
          custom_popup_id.as_mut().unwrap()
        } else {
          // If no custom id provided, then cache the shard as the id
          let id = EguiId::new(self, 0);
          self
            .cached_id
            .get_or_insert_with(|| ui.make_persistent_id(id))
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
        if let Some(inner) =
          egui::popup::popup_above_or_below_widget(ui, *popup_id, &response, above_or_below, |ui| {
            if !self.min_width.get().is_none() {
              ui.set_min_width(self.min_width.get().try_into()?);
            }
            // Activate the contents of the popup with the output of self.widget
            util::activate_ui_contents(
              context,
              &widget_result,
              ui,
              &mut self.parents,
              &mut self.contents,
            )
          })
        {
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
  register_shard::<PopupWrapper>();
}
