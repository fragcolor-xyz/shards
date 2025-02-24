/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

use super::EguiScrollAreaSettings;
use super::LayoutAlign;
use super::LayoutDirection;
use super::LayoutFrame;
use super::ScrollVisibility;
use crate::layouts::LAYOUT_ALIGN_OR_NONE_SLICE;
use crate::layouts::LAYOUT_DIRECTION_OR_NONE_SLICE;
use crate::layouts::SCROLL_VISIBILITY_OR_NONE_SLICE;
use crate::util;
use crate::util::with_possible_panic;
use crate::EguiId;
use crate::ANCHOR_TYPES;
use crate::FLOAT2_VAR_OR_NONE_SLICE;
use crate::FLOAT2_VAR_SLICE;
use crate::FLOAT_VAR_OR_NONE_SLICE;
use crate::LAYOUTCLASS_TYPE;
use crate::LAYOUTCLASS_TYPE_VEC;
use crate::LAYOUTCLASS_TYPE_VEC_VAR;
use crate::LAYOUTCLASS_VAR_OR_NONE_SLICE;
use crate::LAYOUT_FRAME_OR_NONE_SLICE;
use crate::PARENTS_UI_NAME;
use shards::core::register_legacy_shard;
use shards::core::register_shard;
use shards::shard::LegacyShard;
use shards::shard::Shard;
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
use shards::types::BOOL_TYPES;
use shards::types::BOOL_VAR_OR_NONE_SLICE;
use shards::types::SHARDS_OR_NONE_TYPES;
use std::borrow::Borrow;
use std::cell::RefCell;
use std::rc::Rc;

#[derive(shards::shard)]
#[shard_info(
  "UI.Layout2",
  "Basic layout container with direct egui layout parameters."
)]
pub struct Layout2 {
  #[shard_required]
  requiring: ExposedTypes,
  inner_exposed: ExposedTypes,
  #[shard_warmup]
  parents: ParamVar,
  #[shard_param("Contents", "The UI contents to layout.", SHARDS_OR_NONE_TYPES)]
  contents: ShardsVar,
  #[shard_param(
    "MainDirection",
    "Main axis direction (LeftToRight, RightToLeft, TopDown, BottomUp).",
    LAYOUT_DIRECTION_OR_NONE_SLICE
  )]
  main_direction: ParamVar,
  #[shard_param(
    "MainWrap",
    "If true, wrap around when reaching the end of the main direction.",
    BOOL_VAR_OR_NONE_SLICE
  )]
  main_wrap: ParamVar,
  #[shard_param(
    "MainAlign",
    "How to align things on the main axis (Left, Center, Right).",
    LAYOUT_ALIGN_OR_NONE_SLICE
  )]
  main_align: ParamVar,
  #[shard_param(
    "MainJustify",
    "Justify the main axis? For vertical layouts justify means all widgets get maximum width.",
    BOOL_VAR_OR_NONE_SLICE
  )]
  main_justify: ParamVar,
  #[shard_param(
    "CrossAlign",
    "How to align things on the cross axis.",
    LAYOUT_ALIGN_OR_NONE_SLICE
  )]
  cross_align: ParamVar,
  #[shard_param(
    "CrossJustify",
    "Justify the cross axis? For horizontal layouts justify means all widgets get maximum height.",
    BOOL_VAR_OR_NONE_SLICE
  )]
  cross_justify: ParamVar,
}

impl Default for Layout2 {
  fn default() -> Self {
    let mut parents = ParamVar::default();
    parents.set_name(PARENTS_UI_NAME);
    Self {
      requiring: ExposedTypes::new(),
      inner_exposed: ExposedTypes::new(),
      parents,
      contents: ShardsVar::default(),
      main_direction: ParamVar::default(),
      main_wrap: ParamVar::default(),
      main_align: ParamVar::default(),
      main_justify: ParamVar::default(),
      cross_align: ParamVar::default(),
      cross_justify: ParamVar::default(),
    }
  }
}

#[shards::shard_impl]
impl Shard for Layout2 {
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
    // Get current layout as base
    let mut layout = ui.layout().clone();

    // Update layout parameters if provided
    if !self.main_direction.get().is_none() {
      let dir: LayoutDirection = self.main_direction.get().try_into()?;
      layout.main_dir = dir.into();
    }

    if !self.main_wrap.get().is_none() {
      layout.main_wrap = self.main_wrap.get().try_into()?;
    }

    if !self.main_align.get().is_none() {
      let align: LayoutAlign = self.main_align.get().try_into()?;
      layout.main_align = align.into();
    }

    if !self.main_justify.get().is_none() {
      layout.main_justify = self.main_justify.get().try_into()?;
    }

    if !self.cross_align.get().is_none() {
      let align: LayoutAlign = self.cross_align.get().try_into()?;
      layout.cross_align = align.into();
    }

    if !self.cross_justify.get().is_none() {
      layout.cross_justify = self.cross_justify.get().try_into()?;
    }

    // Create child UI with our layout
    ui.with_layout(layout, |ui| {
      util::activate_ui_contents(context, input, ui, &mut self.parents, &mut self.contents)
    })
    .inner?;

    Ok(Some(input.clone()))
  }
}

#[derive(shards::shard)]
#[shard_info("UI.ScrollArea2", "Add scrolling to contained UI elements.")]
pub struct ScrollArea2 {
    #[shard_required]
    requiring: ExposedTypes,
    inner_exposed: ExposedTypes,
    #[shard_warmup]
    parents: ParamVar,
    #[shard_param("Contents", "The UI contents to scroll.", SHARDS_OR_NONE_TYPES)]
    contents: ShardsVar,
    #[shard_param(
        "Horizontal",
        "Enable horizontal scrolling.",
        BOOL_VAR_OR_NONE_SLICE
    )]
    horizontal: ParamVar,
    #[shard_param(
        "Vertical", 
        "Enable vertical scrolling.",
        BOOL_VAR_OR_NONE_SLICE
    )]
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
        BOOL_VAR_OR_NONE_SLICE
    )]
    auto_shrink: ParamVar,
    #[shard_param(
        "MaxHeight",
        "Maximum height of scroll area.",
        FLOAT_VAR_OR_NONE_SLICE
    )]
    max_height: ParamVar,
    #[shard_param(
        "MaxWidth",
        "Maximum width of scroll area.",
        FLOAT_VAR_OR_NONE_SLICE
    )]
    max_width: ParamVar,
}

impl Default for ScrollArea2 {
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
            auto_shrink: ParamVar::new(Var::new_bool(false)),
            max_height: ParamVar::default(),
            max_width: ParamVar::default(),
        }
    }
}

#[shards::shard_impl]
impl Shard for ScrollArea2 {
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
            self.vertical.get().try_into()?
        ]);

        // Configure scroll area
        scroll_area = scroll_area.id_source(EguiId::new(self, 0));

        if !self.max_width.get().is_none() {
            scroll_area = scroll_area.max_width(self.max_width.get().try_into()?);
        }
        if !self.max_height.get().is_none() {
            scroll_area = scroll_area.max_height(self.max_height.get().try_into()?);
        }

        let auto_shrink: bool = if !self.auto_shrink.get().is_none() {
            self.auto_shrink.get().try_into()?
        } else {
            false
        };
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
  register_shard::<Layout2>();
  register_shard::<ScrollArea2>();
}
