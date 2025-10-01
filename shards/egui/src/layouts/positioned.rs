/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

use crate::util;
use crate::PARENTS_UI_NAME;
use egui::{Id, Layout, Pos2, Rect};
use shards::core::register_shard;
use shards::shard::Shard;
use shards::types::{
  Context, ExposedTypes, InstanceData, ParamVar, ShardsVar, Type, Types, Var, ANY_TYPES,
  SHARDS_OR_NONE_TYPES,
};
use shards::types::FLOAT4_TYPES;

#[derive(shards::shard)]
#[shard_info(
  "UI.Positioned",
  "Renders UI contents at an absolute position defined by a Float4 rect (x0, y0, x1, y1)"
)]
struct PositionedShard {
  #[shard_warmup]
  parents: ParamVar,
  #[shard_required]
  required: ExposedTypes,
  #[shard_param("Contents", "The UI contents to render.", SHARDS_OR_NONE_TYPES)]
  contents: ShardsVar,
  inner_exposed: ExposedTypes,
}

impl Default for PositionedShard {
  fn default() -> Self {
    Self {
      parents: ParamVar::new_named(PARENTS_UI_NAME),
      required: ExposedTypes::new(),
      contents: ShardsVar::default(),
      inner_exposed: ExposedTypes::new(),
    }
  }
}

#[shards::shard_impl]
impl Shard for PositionedShard {
  fn input_types(&mut self) -> &Types {
    &FLOAT4_TYPES
  }

  fn output_types(&mut self) -> &Types {
    &ANY_TYPES
  }

  fn warmup(&mut self, ctx: &Context) -> Result<(), &str> {
    self.warmup_helper(ctx)?;
    Ok(())
  }

  fn cleanup(&mut self, ctx: Option<&Context>) -> Result<(), &str> {
    self.cleanup_helper(ctx)?;
    Ok(())
  }

  fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
    self.compose_helper(data)?;
    util::require_parents(&mut self.required);

    if !self.contents.is_empty() {
      let composed = self.contents.compose(data)?;
      shards::util::merge_exposed_types(&mut self.inner_exposed, &composed.exposedInfo);
      shards::util::merge_exposed_types(&mut self.required, &composed.requiredInfo);
    }

    Ok(data.inputType)
  }

  fn activate(&mut self, context: &Context, input: &Var) -> Result<Option<Var>, &str> {
    if self.contents.is_empty() {
      return Ok(Some(input.clone()));
    }

    let ui = util::get_parent_ui(self.parents.get())?;

    // Extract Float4 rect from input (x0, y0, x1, y1)
    let rect_values = unsafe { input.payload.__bindgen_anon_1.float4Value };
    let rect = Rect::from_min_max(
      Pos2::new(rect_values[0] as f32, rect_values[1] as f32),
      Pos2::new(rect_values[2] as f32, rect_values[3] as f32),
    );

    // Create a child UI at the specified rect
    let id_src = Id::new("UI.Positioned").with(ui.id());
    let child_layout = Layout::top_down(egui::Align::Min);
    let mut child_ui = ui.child_ui_with_id_source(rect, child_layout, id_src, None);

    // Render contents in the child UI
    let result =
      util::activate_ui_contents(context, input, &mut child_ui, &mut self.parents, &mut self.contents)?;

    Ok(Some(result))
  }
}

pub fn register_shards() {
  register_shard::<PositionedShard>();
}