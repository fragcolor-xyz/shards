/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

use super::RenderTarget;
use crate::util;
use crate::widgets::image_util;
use crate::CONTEXTS_NAME;
use crate::FLOAT2_VAR_SLICE;
use crate::HELP_OUTPUT_EQUAL_INPUT;
use crate::PARENTS_UI_NAME;
use egui::load::SizedTexture;
use shards::shard::LegacyShard;
use shards::shardsc::SHType_Object as SHTYPE_OBJECT;
use shards::types::Context;
use shards::types::ExposedTypes;
use shards::types::InstanceData;
use shards::types::OptionalString;
use shards::types::ParamVar;
use shards::types::Parameters;
use shards::types::Type;
use shards::types::Types;
use shards::types::Var;

lazy_static! {
  static ref TEXTURE_TYPES: Vec<Type> = vec![*image_util::TEXTURE_TYPE];
  static ref RENDER_TARGET_PARAMETERS: Parameters = vec![(
    cstr!("Scale"),
    shccstr!("Scaling to apply to the source texture."),
    FLOAT2_VAR_SLICE,
  )
    .into(),];
}

impl Default for RenderTarget {
  fn default() -> Self {
    Self {
      parents: ParamVar::new_named(PARENTS_UI_NAME),
      contexts: ParamVar::new_named(CONTEXTS_NAME),
      requiring: Vec::new(),
      scale: ParamVar::new((1.0, 1.0).into()),
    }
  }
}

impl LegacyShard for RenderTarget {
  fn registerName() -> &'static str
  where
    Self: Sized,
  {
    cstr!("UI.RenderTarget")
  }

  fn hash() -> u32
  where
    Self: Sized,
  {
    compile_time_crc32::crc32!("UI.RenderTarget-rust-0x20200101")
  }

  fn name(&mut self) -> &str {
    "UI.RenderTarget"
  }

  fn help(&mut self) -> OptionalString {
    OptionalString(shccstr!(
      "Display the contents of a render target. Consumes input on the region."
    ))
  }

  fn inputTypes(&mut self) -> &Types {
    &TEXTURE_TYPES
  }

  fn inputHelp(&mut self) -> OptionalString {
    OptionalString(shccstr!("The image to display."))
  }

  fn outputTypes(&mut self) -> &Types {
    &TEXTURE_TYPES
  }

  fn outputHelp(&mut self) -> OptionalString {
    *HELP_OUTPUT_EQUAL_INPUT
  }

  fn parameters(&mut self) -> Option<&Parameters> {
    Some(&RENDER_TARGET_PARAMETERS)
  }

  fn setParam(&mut self, index: i32, value: &Var) -> Result<(), &str> {
    match index {
      0 => self.scale.set_param(value),
      _ => Err("Invalid parameter index"),
    }
  }

  fn getParam(&mut self, index: i32) -> Var {
    match index {
      0 => self.scale.get_param(),
      _ => Var::default(),
    }
  }

  fn requiredVariables(&mut self) -> Option<&ExposedTypes> {
    self.requiring.clear();

    // Add UI.Parents to the list of required variables
    util::require_parents(&mut self.requiring);

    Some(&self.requiring)
  }

  fn hasCompose() -> bool {
    true
  }

  fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
    match data.inputType.basicType {
      SHTYPE_OBJECT
        if unsafe { data.inputType.details.object.typeId } == image_util::TEXTURE_CC =>
      {
        decl_override_activate! {
          data.activate = RenderTarget::texture_activate;
        }
      }
      _ => return Err("Invalid input type"),
    }
    // Always passthrough the input
    Ok(data.inputType)
  }

  fn warmup(&mut self, context: &Context) -> Result<(), &str> {
    self.contexts.warmup(context);
    self.parents.warmup(context);
    self.scale.warmup(context);

    Ok(())
  }

  fn cleanup(&mut self, ctx: Option<&Context>) -> Result<(), &str> {
    self.scale.cleanup(ctx);
    self.parents.cleanup(ctx);
    self.contexts.cleanup(ctx);

    Ok(())
  }

  fn activate(&mut self, _context: &Context, _input: &Var) -> Result<Var, &str> {
    Err("Invalid input type")
  }
}

impl RenderTarget {
  fn activate_texture(&mut self, _context: &Context, input: &Var) -> Result<Var, &str> {
    let ctx = util::get_current_context(&self.contexts)?;
    if let Some(ui) = util::get_current_parent_opt(self.parents.get())? {
      let (texture_id, texture_size) = image_util::get_egui_texture_from_gfx(ctx, input)?;

      let scale = image_util::into_vec2(&self.scale)? / ui.ctx().pixels_per_point();

      // Manually allocate region to consume input events
      let (rect, _response) =
        ui.allocate_exact_size(texture_size * scale, egui::Sense::click_and_drag());

      // Draw texture at this rectangle
      let img = SizedTexture {
        id: texture_id,
        size: texture_size,
      };
      let image = egui::widgets::Image::new(img);
      image.paint_at(ui, rect);

      Ok(*input)
    } else {
      Err("No UI parent")
    }
  }

  impl_legacy_override_activate! {
    extern "C" fn texture_activate() -> *const Var {
      RenderTarget::activate_texture()
    }
  }
}
