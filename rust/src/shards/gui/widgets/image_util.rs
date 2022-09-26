/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

use egui::TextureHandle;
use egui::TextureId;
use egui::Ui;
use egui::Vec2;
use std::ptr::null_mut;

use crate::fourCharacterCode;
use crate::shardsc::gfx_TexturePtr;
use crate::shardsc::gfx_TexturePtr_getResolution_ext;
use crate::shardsc::SHImage;
use crate::shardsc::SHType_Image;
use crate::shardsc::SHType_Object;
use crate::shardsc::SHIMAGE_FLAGS_PREMULTIPLIED_ALPHA;
use crate::types::common_type;
use crate::types::Context;
use crate::types::ExposedTypes;
use crate::types::InstanceData;
use crate::types::OptionalString;
use crate::types::ParamVar;
use crate::types::Parameters;
use crate::types::Type;
use crate::types::Types;
use crate::types::Var;
use crate::types::FRAG_CC;

pub const TextureCC: i32 = fourCharacterCode(*b"tex_");

lazy_static! {
  pub static ref TEXTURE_TYPE: Type = Type::object(FRAG_CC, TextureCC);
  pub static ref TEXTURE_OR_IMAGE_TYPES: Vec<Type> = vec![common_type::image, *TEXTURE_TYPE];
}

pub fn get_scale(scale_var: &ParamVar) -> Result<egui::Vec2, &'static str> {
  let scale: (f32, f32) = scale_var.get().try_into()?;
  Ok(egui::vec2(scale.0, scale.1))
}

pub struct CachedUIImage {
  texture_handle: Option<TextureHandle>,
  prev_ptr: *mut u8,
}

impl Default for CachedUIImage {
  fn default() -> Self {
    Self {
      texture_handle: None,
      prev_ptr: null_mut(),
    }
  }
}

pub fn ui_image_cached<'a>(
  cached_image: &'a mut CachedUIImage,
  input: &Var,
  ui: &Ui,
) -> Result<&'a egui::TextureHandle, &'static str> {
  let shimage: &SHImage = input.try_into()?;
  let ptr = shimage.data;
  Ok(if ptr != cached_image.prev_ptr {
    let image: egui::ColorImage = shimage.into();
    cached_image.prev_ptr = ptr;
    cached_image.texture_handle.insert(ui.ctx().load_texture(
      format!("UI.Image: {:p}", shimage.data),
      image,
      Default::default(),
    ))
  } else {
    cached_image.texture_handle.as_ref().unwrap()
  })
}

pub fn ui_image_texture(input: &Var) -> Result<(TextureId, Vec2), &'static str> {
  let ptr = unsafe { input.payload.__bindgen_anon_1.__bindgen_anon_1.objectValue as u64 };

  let texture_ptr = Var::from_object_ptr_mut_ref::<gfx_TexturePtr>(*input, &TEXTURE_TYPE)?;
  let texture_size = {
    let texture_res = unsafe { gfx_TexturePtr_getResolution_ext(texture_ptr) };
    egui::vec2(texture_res.x as f32, texture_res.y as f32)
  };

  Ok((egui::epaint::TextureId::User(ptr), texture_size))
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
