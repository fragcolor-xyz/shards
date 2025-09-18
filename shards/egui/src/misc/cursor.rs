/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

use shards::core::register_shard;
use shards::shard::Shard;
use shards::types::{Context, ExposedTypes, InstanceData, ParamVar, Type, Types, Var};
use shards::types::FLOAT2_TYPES;
use crate::{util, PARENTS_UI_NAME};

#[derive(shards::shard)]
#[shard_info("UI.SetCursor", "Sets the cursor position to a specific UI coordinate (Float2)")]
struct SetCursorShard {
  #[shard_warmup]
  parents: ParamVar,
  #[shard_required]
  required: ExposedTypes,
}

impl Default for SetCursorShard {
  fn default() -> Self {
    Self {
      parents: ParamVar::new_named(PARENTS_UI_NAME),
      required: ExposedTypes::new(),
    }
  }
}

#[shards::shard_impl]
impl Shard for SetCursorShard {
  fn input_types(&mut self) -> &Types {
    &FLOAT2_TYPES
  }

  fn output_types(&mut self) -> &Types {
    &FLOAT2_TYPES
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
    Ok(self.output_types()[0])
  }

  fn activate(&mut self, _context: &Context, input: &Var) -> Result<Option<Var>, &str> {
    let ui = util::get_parent_ui(self.parents.get())?;

    // Extract Float2 from input
    let pos = unsafe { input.payload.__bindgen_anon_1.float2Value };
    let target_pos = egui::pos2(pos[0] as f32, pos[1] as f32);

    // Calculate the offset from current cursor position to target position
    let current_pos = ui.cursor().min;
    let offset = target_pos - current_pos;

    // Only allocate space if we're moving forward (positive offset)
    // For backwards movement, we can't move the cursor back, so we just skip
    if offset.x > 0.0 || offset.y > 0.0 {
      ui.allocate_space(egui::vec2(offset.x.max(0.0), offset.y.max(0.0)));
    }

    Ok(Some(input.clone()))
  }
}

pub fn register_shards() {
  register_shard::<SetCursorShard>();
}