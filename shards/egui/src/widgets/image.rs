/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

use super::image_util;
use super::Image;
use crate::util;
use crate::FLOAT2_VAR_SLICE;

use crate::HELP_OUTPUT_EQUAL_INPUT;
use crate::PARENTS_UI_NAME;
use shards::shard::LegacyShard;
use shards::shardsc::SHType_Image;
use shards::shardsc::SHType_Object;

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
  static ref IMAGE_PARAMETERS: Parameters = vec![(
    cstr!("Scale"),
    shccstr!("Scaling to apply to the source image."),
    FLOAT2_VAR_SLICE,
  )
    .into(),];
}

impl Default for Image {
  fn default() -> Self {
    let mut parents = ParamVar::default();
    parents.set_name(PARENTS_UI_NAME);
    Self {
      parents,
      requiring: Vec::new(),
      scale: ParamVar::new((1.0, 1.0).into()),
      cached_ui_image: Default::default(),
      current_version: 0,
    }
  }
}

impl LegacyShard for Image {
  fn registerName() -> &'static str
  where
    Self: Sized,
  {
    cstr!("UI.Image")
  }

  fn hash() -> u32
  where
    Self: Sized,
  {
    compile_time_crc32::crc32!("UI.Image-rust-0x20200101")
  }

  fn name(&mut self) -> &str {
    "UI.Image"
  }

  fn help(&mut self) -> OptionalString {
    OptionalString(shccstr!("Display an image in the UI."))
  }

  fn inputTypes(&mut self) -> &Types {
    &image_util::TEXTURE_OR_IMAGE_TYPES
  }

  fn inputHelp(&mut self) -> OptionalString {
    OptionalString(shccstr!("The image to display."))
  }

  fn outputTypes(&mut self) -> &Types {
    &image_util::TEXTURE_OR_IMAGE_TYPES
  }

  fn outputHelp(&mut self) -> OptionalString {
    *HELP_OUTPUT_EQUAL_INPUT
  }

  fn parameters(&mut self) -> Option<&Parameters> {
    Some(&IMAGE_PARAMETERS)
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
      SHType_Image => decl_override_activate! {
        data.activate = Image::image_activate;
      },
      SHType_Object
        if unsafe { data.inputType.details.object.typeId } == image_util::TEXTURE_CC =>
      {
        decl_override_activate! {
          data.activate = Image::texture_activate;
        }
      }
      _ => (),
    }
    // Always passthrough the input
    Ok(data.inputType)
  }

  fn warmup(&mut self, context: &Context) -> Result<(), &str> {
    self.parents.warmup(context);
    self.scale.warmup(context);

    self.current_version = 0;

    Ok(())
  }

  fn cleanup(&mut self) -> Result<(), &str> {
    self.scale.cleanup();
    self.parents.cleanup();

    Ok(())
  }

  fn activate(&mut self, _context: &Context, _input: &Var) -> Result<Var, &str> {
    Err("Invalid input type")
  }
}

impl Image {
  fn activateImage(&mut self, _context: &Context, input: &Var) -> Result<Var, &str> {
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

      let scale = image_util::get_scale(&self.scale)?;
      ui.image(texture, texture.size_vec2() * scale);

      Ok(*input)
    } else {
      Err("No UI parent")
    }
  }

  fn activateTexture(&mut self, _context: &Context, input: &Var) -> Result<Var, &str> {
    if let Some(ui) = util::get_current_parent_opt(self.parents.get())? {
      let (texture_id, texture_size) = image_util::get_egui_texture_from_gfx(input)?;

      let scale = image_util::get_scale(&self.scale)?;
      ui.image(texture_id, texture_size * scale);

      Ok(*input)
    } else {
      Err("No UI parent")
    }
  }

  impl_override_activate! {
    extern "C" fn image_activate() -> Var {
      Image::activateImage()
    }
  }

  impl_override_activate! {
    extern "C" fn texture_activate() -> Var {
      Image::activateTexture()
    }
  }
}
