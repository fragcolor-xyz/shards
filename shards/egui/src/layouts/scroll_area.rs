/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

use crate::util;
use crate::EguiId;
use crate::FLOAT_VAR_OR_NONE_SLICE;
use crate::PARENTS_UI_NAME;
use shards::core::register_shard;
use shards::shard::Shard;
use shards::types::Context;
use shards::types::ExposedTypes;
use shards::types::InstanceData;
use shards::types::ParamVar;
use shards::types::ShardsVar;
use shards::types::Type;
use shards::types::Types;
use shards::types::Var;
use shards::types::ANY_TYPES;
use shards::types::BOOL_VAR_OR_NONE_SLICE;
use shards::types::SHARDS_OR_NONE_TYPES;
use shards::types::BOOL_OR_VAR_SLICE;

#[derive(shards::shard)]
#[shard_info("UI.ScrollArea", "Add scrolling to contained UI elements.")]
pub struct ScrollArea {
  #[shard_required]
  requiring: ExposedTypes,
  inner_exposed: ExposedTypes, 
  #[shard_warmup]
  parents: ParamVar,
  #[shard_param("Contents", "The UI contents to scroll.", SHARDS_OR_NONE_TYPES)]
  contents: ShardsVar,
  #[shard_param("Horizontal", "Enable horizontal scrolling.", BOOL_VAR_OR_NONE_SLICE)]
  horizontal: ParamVar,
  #[shard_param("Vertical", "Enable vertical scrolling.", BOOL_VAR_OR_NONE_SLICE)]
  vertical: ParamVar,
  #[shard_param(
    "AlwaysShow",
    "Always show the enabled scroll bars even if not needed.",
    BOOL_VAR_OR_NONE_SLICE
  )]
  always_show: ParamVar,
  #[shard_param(
    "AutoShrink",
    "Whether to automatically shrink the scroll area.",
    BOOL_OR_VAR_SLICE
  )]
  auto_shrink: ParamVar,
  #[shard_param("MaxHeight", "Maximum height of scroll area.", FLOAT_VAR_OR_NONE_SLICE)]
  max_height: ParamVar,
  #[shard_param("MaxWidth", "Maximum width of scroll area.", FLOAT_VAR_OR_NONE_SLICE)]
  max_width: ParamVar,
}

impl Default for ScrollArea {
  fn default() -> Self {
    let mut parents = ParamVar::default();
    parents.set_name(PARENTS_UI_NAME);
    Self {
      requiring: ExposedTypes::new(),
      inner_exposed: ExposedTypes::new(),
      parents,
      contents: ShardsVar::default(),
      horizontal: ParamVar::new(Var::new_bool(false)),
      vertical: ParamVar::new(Var::new_bool(true)),
      always_show: ParamVar::new(Var::new_bool(false)),
      auto_shrink: ParamVar::new(Var::new_bool(true)),
      max_height: ParamVar::default(),
      max_width: ParamVar::default(),
    }
  }
}

#[shards::shard_impl]
impl Shard for ScrollArea {
  fn input_types(&mut self) -> &Types {
    &ANY_TYPES
  }

  fn output_types(&mut self) -> &Types {
    &ANY_TYPES
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
    util::require_parents(&mut self.requiring);

    if !self.contents.is_empty() {
      let composed = self.contents.compose(data)?;
      shards::util::merge_exposed_types(&mut self.inner_exposed, &composed.exposedInfo);
      shards::util::merge_exposed_types(&mut self.requiring, &composed.requiredInfo);
    }

    Ok(data.inputType)
  }

  fn activate(&mut self, context: &Context, input: &Var) -> Result<Option<Var>, &str> {
    let ui = util::get_parent_ui(self.parents.get())?;

    let mut scroll_area = egui::ScrollArea::new([
      self.horizontal.get().try_into()?,
      self.vertical.get().try_into()?,
    ]).id_source(EguiId::new(self, 0));

    // Configure scroll area
    scroll_area = scroll_area.id_source(EguiId::new(self, 0));

    if !self.max_width.get().is_none() {
      scroll_area = scroll_area.max_width(self.max_width.get().try_into()?);
    }
    if !self.max_height.get().is_none() {
      scroll_area = scroll_area.max_height(self.max_height.get().try_into()?);
    }

    let auto_shrink: bool = self.auto_shrink.get().try_into()?;
    scroll_area = scroll_area.auto_shrink([auto_shrink; 2]);

    let visibility = if self.always_show.get().try_into()? {
      egui::scroll_area::ScrollBarVisibility::AlwaysVisible
    } else {
      egui::scroll_area::ScrollBarVisibility::VisibleWhenNeeded
    };
    scroll_area = scroll_area.scroll_bar_visibility(visibility);

    // Show scroll area with contents
    scroll_area
      .show(ui, |ui| {
        util::activate_ui_contents(context, input, ui, &mut self.parents, &mut self.contents)
      })
      .inner?;

    Ok(Some(input.clone()))
  }
}

pub(crate) fn register_shards() {
  register_shard::<ScrollArea>();
}
