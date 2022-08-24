/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

use super::Image;
use crate::shard::Shard;
use crate::shards::gui::util;
use crate::shards::gui::widgets::FLOAT2_VAR_SLICE;
use crate::shards::gui::PARENTS_UI_NAME;
use crate::shardsc::SHImage;
use crate::shardsc::SHType_Image;
use crate::shardsc::SHIMAGE_FLAGS_PREMULTIPLIED_ALPHA;
use crate::types::Context;
use crate::types::ExposedTypes;
use crate::types::OptionalString;
use crate::types::ParamVar;
use crate::types::Parameters;
use crate::types::Types;
use crate::types::Var;
use crate::types::IMAGE_TYPES;

lazy_static! {
  static ref IMAGE_PARAMETERS: Parameters = vec![(
    cstr!("Scale"),
    cstr!("Scaling to apply to the source image"),
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
      texture: None,
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
    &IMAGE_TYPES
  }

  fn inputHelp(&mut self) -> OptionalString {
    OptionalString(shccstr!("The image to display"))
  }

  fn outputTypes(&mut self) -> &Types {
    &IMAGE_TYPES
  }

  fn outputHelp(&mut self) -> OptionalString {
    OptionalString(shccstr!("The output of this shard will be its input."))
  }

  fn parameters(&mut self) -> Option<&Parameters> {
    Some(&IMAGE_PARAMETERS)
  }

  fn setParam(&mut self, index: i32, value: &Var) -> Result<(), &str> {
    match index {
      0 => Ok(self.scale.set_param(value)),
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
    util::require_parents(&mut self.requiring, &self.parents);

    Some(&self.requiring)
  }

  fn warmup(&mut self, context: &Context) -> Result<(), &str> {
    self.parents.warmup(context);
    self.scale.warmup(context);

    Ok(())
  }

  fn cleanup(&mut self) -> Result<(), &str> {
    self.scale.cleanup();
    self.parents.cleanup();

    Ok(())
  }

  fn activate(&mut self, _context: &Context, input: &Var) -> Result<Var, &str> {
    if let Some(ui) = util::get_current_parent(*self.parents.get())? {
      let texture = &*self.texture.get_or_insert_with(|| {
        let image: &SHImage = input.try_into().unwrap();
        let image: egui::ColorImage = image.into();

        ui.ctx().load_texture("example", image) // FIXME name
      });

      let scale: (f32, f32) = self.scale.get().try_into()?;
      let scale: egui::Vec2 = scale.into();
      ui.image(texture, scale * texture.size_vec2());

      Ok(*input)
    } else {
      Err("No UI parent")
    }
  }
}

impl From<&SHImage> for egui::ColorImage {
  fn from(image: &SHImage) -> egui::ColorImage {
    assert_eq!(image.channels, 4);

    let size = [image.width as _, image.height as _];
    let rgba = unsafe {
      core::slice::from_raw_parts(
        image.data,
        image.width as usize * image.channels as usize * image.height as usize,
      )
    };

    if image.flags & SHIMAGE_FLAGS_PREMULTIPLIED_ALPHA as u8
      == SHIMAGE_FLAGS_PREMULTIPLIED_ALPHA as u8
    {
      let pixels = rgba
        .chunks_exact(4)
        .map(|p| egui::Color32::from_rgba_premultiplied(p[0], p[1], p[2], p[3]))
        .collect();
      Self { size, pixels }
    } else {
      egui::ColorImage::from_rgba_unmultiplied(size, rgba)
    }
  }
}
