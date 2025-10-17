/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

use crate::util;
use crate::FLOAT_VAR_OR_NONE_SLICE;
use crate::PARENTS_UI_NAME;
use egui::vec2;
use egui::Ui;
use egui::Vec2;
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

#[derive(shards::shard)]
#[shard_info("UI.Sized", "Layout the contents with specific dimensions.")]
pub struct SizedShard {
  #[shard_required]
  requiring: ExposedTypes,
  inner_exposed: ExposedTypes,
  #[shard_warmup]
  parents: ParamVar,
  #[shard_param("Contents", "The UI contents.", SHARDS_OR_NONE_TYPES)]
  contents: ShardsVar,
  #[shard_param("Width", "Width of the sized container.", FLOAT_VAR_OR_NONE_SLICE)]
  width: ParamVar,
  #[shard_param("Height", "Height of the sized container.", FLOAT_VAR_OR_NONE_SLICE)]
  height: ParamVar,
  #[shard_param("FillWidth", "Fill the available width.", BOOL_VAR_OR_NONE_SLICE)]
  fill_width: ParamVar,
  #[shard_param("FillHeight", "Fill the available height.", BOOL_VAR_OR_NONE_SLICE)]
  fill_height: ParamVar,
}

impl Default for SizedShard {
  fn default() -> Self {
    let mut parents = ParamVar::default();
    parents.set_name(PARENTS_UI_NAME);
    Self {
      requiring: ExposedTypes::new(),
      inner_exposed: ExposedTypes::new(),
      parents,
      contents: ShardsVar::default(),
      width: ParamVar::default(),
      height: ParamVar::default(),
      fill_width: ParamVar::new(Var::new_bool(false)),
      fill_height: ParamVar::new(Var::new_bool(false)),
    }
  }
}

#[shards::shard_impl]
impl Shard for SizedShard {
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
    if self.contents.is_empty() {
      return Ok(Some(input.clone()));
    }

    let ui = util::get_parent_ui(self.parents.get())?;

    // Get width and height values, defaulting to 0.0 if None
    let width_val = if self.width.get().is_none() {
      0.0
    } else {
      self.width.get().try_into()?
    };
    
    let height_val = if self.height.get().is_none() {
      0.0
    } else {
      self.height.get().try_into()?
    };

    let mut max_size = vec2(width_val, height_val);
    let min_size = ui.min_size();
    if max_size.x < min_size.x {
      max_size.x = min_size.x;
    }
    if max_size.y < min_size.y {
      max_size.y = min_size.y;
    }

    let mut sized: Vec2 = max_size;
    let available_size = ui.available_size();
    let fill_width: bool = self.fill_width.get().try_into()?;
    let fill_height: bool = self.fill_height.get().try_into()?;
    if fill_width {
      sized.x = available_size.x;
    }
    if fill_height {
      sized.y = available_size.y;
    }

    let mut resp: Result<Var, &str> = Err("undefined");
    ui.add_sized(sized, |ui: &mut Ui| {
      ui.set_min_size(vec2(0.0, 0.0));
      if max_size.x > 0.0 {
        ui.set_max_width(max_size.x);
      }
      if max_size.y > 0.0 {
        ui.set_max_height(max_size.y);
      }
      if fill_width {
        ui.set_width(available_size.x);
      } else {
        ui.set_width(max_size.x);
      }
      if fill_height {
        ui.set_height(available_size.y);
      } else {
        ui.set_height(max_size.y);
      }
      let response = ui
        .with_layout(
          ui.layout().clone(),
          |ui| {
            resp =
              util::activate_ui_contents(context, input, ui, &mut self.parents, &mut self.contents);
          },
        )
        .response;
      ui.allocate_space(ui.available_size());
      response
    });

    resp?;

    Ok(Some(input.clone()))
  }
}

pub(crate) fn register_shards() {
  register_shard::<SizedShard>();
}
