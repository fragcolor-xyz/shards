/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

use super::image_util;
use super::image_util::AutoTexturePtr;
use crate::util;
use crate::FLOAT2_VAR_SLICE;
use crate::HELP_OUTPUT_EQUAL_INPUT;
use crate::PARENTS_UI_NAME;
use egui::load::SizedTexture;
use shards::core::register_shard;
use shards::shard::Shard;
use shards::shardsc::SHType_Image;
use shards::shardsc::SHType_Object;
use shards::types::Context;
use shards::types::ExposedTypes;
use shards::types::InstanceData;
use shards::types::OptionalString;
use shards::types::ParamVar;
use shards::types::Type;
use shards::types::Types;
use shards::types::Var;
use shards::types::BOOL_VAR_OR_NONE_SLICE;

#[derive(shard)]
#[shard_info("UI.Image", "Display an image in the UI.")]
struct Image {
  #[shard_warmup]
  parents: ParamVar,
  #[shard_param("Scale", "Scaling to apply to the source image.", FLOAT2_VAR_SLICE)]
  scale: ParamVar,
  // This will be in UI points (ScalingAware: false) or in pixels (ScalingAware: true)
  #[shard_param("Size", "The size to render the image at.", FLOAT2_VAR_SLICE)]
  size: ParamVar,
  // Defaults to false
  #[shard_param("ScalingAware", "When set to true, this image's pixels will be rendered 1:1 regardless of UI context point size.", BOOL_VAR_OR_NONE_SLICE)]
  scaling_aware: ParamVar,
  #[shard_required]
  requiring: ExposedTypes,
  cached_ui_image: image_util::CachedUIImage,
  current_version: u64,
  step_textures: Vec<AutoTexturePtr>,
  step_counter: u64,
}

impl Default for Image {
  fn default() -> Self {
    Self {
      parents: ParamVar::new_named(PARENTS_UI_NAME),
      requiring: Vec::new(),
      scale: ParamVar::new((1.0, 1.0).into()),
      size: ParamVar::default(),
      scaling_aware: ParamVar::default(),
      cached_ui_image: Default::default(),
      current_version: 0,
      step_textures: Vec::new(),
      step_counter: 0,
    }
  }
}

#[shard_impl]
impl Shard for Image {
  fn input_types(&mut self) -> &Types {
    &image_util::TEXTURE_OR_IMAGE_TYPES
  }

  fn input_help(&mut self) -> OptionalString {
    OptionalString(shccstr!("The image to display."))
  }

  fn output_types(&mut self) -> &Types {
    &image_util::TEXTURE_OR_IMAGE_TYPES
  }

  fn output_help(&mut self) -> OptionalString {
    *HELP_OUTPUT_EQUAL_INPUT
  }

  fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
    self.compose_helper(data)?;
    util::require_parents(&mut self.requiring);

    match data.inputType.basicType {
      SHType_Image => decl_override_activate! {
        data.activate = Image::activate_image_override;
      },
      SHType_Object
        if unsafe { data.inputType.details.object.typeId } == image_util::TEXTURE_CC =>
      {
        decl_override_activate! {
          data.activate = Image::activate_texture_override;
        }
      }
      _ => return Err("Invalid input type"),
    }
    // Always passthrough the input
    Ok(data.inputType)
  }

  fn warmup(&mut self, context: &Context) -> Result<(), &str> {
    self.warmup_helper(context)?;

    self.current_version = 0;

    Ok(())
  }

  fn cleanup(&mut self, ctx: Option<&Context>) -> Result<(), &str> {
    self.cleanup_helper(ctx)?;

    self.step_textures.clear();
    self.step_counter = 0;

    Ok(())
  }

  fn activate(&mut self, _context: &Context, _input: &Var) -> Result<Var, &str> {
    Err("Invalid input type")
  }
}

impl Image {
  fn activate_image(&mut self, _context: &Context, input: &Var) -> Result<Var, &str> {
    let version = unsafe { input.__bindgen_anon_1.version };
    if self.current_version != version {
      self.cached_ui_image.invalidate();
      self.current_version = version;
      shlog_debug!("Image version changed: {}", self.current_version);
    }

    if let Some(ui) = util::get_current_parent_opt(self.parents.get())? {
      let texture = self
        .cached_ui_image
        .get_egui_texture_from_image(input, ui)?;

      let size = image_util::resolve_image_size(
        ui,
        &self.size,
        &self.scale,
        &self.scaling_aware,
        texture.size_vec2(),
      );
      let img = SizedTexture {
        id: texture.id(),
        size,
      };
      ui.image(img);

      Ok(*input)
    } else {
      Err("No UI parent")
    }
  }

  fn activate_texture(&mut self, context: &Context, input: &Var) -> Result<Var, &str> {
    // support rendering multiple images from same shard
    let current_step = shards::core::step_count(context);
    if self.step_counter != current_step {
      self.step_counter = current_step;
      self.step_textures.clear();
    }

    if let Some(ui) = util::get_current_parent_opt(self.parents.get())? {
      let (texture_id, texture_size) =
        image_util::get_egui_texture_from_gfx(input, &mut self.step_textures)?;

      let size = image_util::resolve_image_size(
        ui,
        &self.size,
        &self.scale,
        &self.scaling_aware,
        texture_size,
      );

      let img = SizedTexture {
        id: texture_id,
        size,
      };
      ui.image(img);

      Ok(*input)
    } else {
      Err("No UI parent")
    }
  }

  impl_override_activate! {
    extern "C" fn activate_image_override() -> Var {
      Image::activate_image()
    }
  }

  impl_override_activate! {
    extern "C" fn activate_texture_override() -> Var {
      Image::activate_texture()
    }
  }
}

pub fn register_shards() {
  register_shard::<Image>();
}
