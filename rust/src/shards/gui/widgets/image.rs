/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

use super::image_util;
use super::Image;
use crate::shard::Shard;
use crate::shards::gui::util;
use crate::shards::gui::FLOAT2_VAR_SLICE;
use crate::shards::gui::HASH_VAR_OR_NONE_SLICE;
use crate::shards::gui::HELP_OUTPUT_EQUAL_INPUT;
use crate::shards::gui::PARENTS_UI_NAME;
use crate::shardsc::SHType_Image;
use crate::shardsc::SHType_Object;
use crate::types::Context;
use crate::types::ExposedInfo;
use crate::types::ExposedTypes;
use crate::types::InstanceData;
use crate::types::OptionalString;
use crate::types::ParamVar;
use crate::types::Parameters;
use crate::types::Type;
use crate::types::Types;
use crate::types::Var;
use crate::types::common_type;

lazy_static! {
  static ref IMAGE_PARAMETERS: Parameters = vec![
    (
      cstr!("Scale"),
      shccstr!("Scaling to apply to the source image."),
      FLOAT2_VAR_SLICE,
    )
      .into(),
    (
      cstr!("Hash"),
      shccstr!(
        "Optional hash variable used to invalidate cached data in case the contents of the image change."
      ),
      HASH_VAR_OR_NONE_SLICE,
    )
      .into()
  ];
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
      hash: ParamVar::default(),
      current_hash: Var::default(),
    }
  }
}

impl Shard for Image {
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
      0 => Ok(self.scale.set_param(value)),
      1 => Ok(self.hash.set_param(value)),
      _ => Err("Invalid parameter index"),
    }
  }

  fn getParam(&mut self, index: i32) -> Var {
    match index {
      0 => self.scale.get_param(),
      1 => self.hash.get_param(),
      _ => Var::default(),
    }
  }

  fn requiredVariables(&mut self) -> Option<&ExposedTypes> {
    self.requiring.clear();

    if self.hash.is_variable() {
      let hash_info = ExposedInfo {
        exposedType: common_type::int2,
        name: self.hash.get_name(),
        help: cstr!("The hash variable.").into(),
        ..ExposedInfo::default()
      };
      self.requiring.push(hash_info);
    }

    // Add UI.Parents to the list of required variables
    util::require_parents(&mut self.requiring, &self.parents);

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
    self.hash.warmup(context);

    Ok(())
  }

  fn cleanup(&mut self) -> Result<(), &str> {
    self.scale.cleanup();
    self.parents.cleanup();
    self.hash.cleanup();

    Ok(())
  }

  fn activate(&mut self, _context: &Context, _input: &Var) -> Result<Var, &str> {
    Err("Invalid input type")
  }
}

impl Image {
  fn activateImage(&mut self, _context: &Context, input: &Var) -> Result<Var, &str> {
    let v_hash = self.hash.get();
    if !v_hash.is_none() {
      if self.current_hash != *v_hash {
        self.cached_ui_image.invalidate();
        self.current_hash = *v_hash;
        shlog_debug!("Image hash changed: {:?}", self.current_hash)
      }
    }

    if let Some(ui) = util::get_current_parent(self.parents.get())? {
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
    let v_hash = self.hash.get();
    if !v_hash.is_none() {
      if self.current_hash != *v_hash {
        self.cached_ui_image.invalidate();
        self.current_hash = *v_hash;
        shlog_debug!("Image hash changed: {:?}", self.current_hash)
      }
    }

    if let Some(ui) = util::get_current_parent(self.parents.get())? {
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
