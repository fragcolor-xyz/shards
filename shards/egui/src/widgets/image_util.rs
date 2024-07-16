/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

use crate::bindings::gfx_TexturePtr;
use crate::bindings::gfx_TexturePtr_getResolution_ext;
use crate::bindings::gfx_TexturePtr_refAt;
use crate::bindings::gfx_TexturePtr_unrefAt;
use crate::bindings::linalg_aliases_int2;
use crate::bindings::GenericSharedPtr;
use egui::vec2;
use shards::fourCharacterCode;
use shards::shardsc::SHImage;
use shards::shardsc::SHIMAGE_FLAGS_PREMULTIPLIED_ALPHA;
use shards::types::common_type;
use shards::types::ParamVar;
use shards::types::Type;
use shards::types::Var;
use shards::types::FRAG_CC;
use shards::util::from_raw_parts_allow_null;
use std::ptr::null_mut;

pub const TEXTURE_CC: i32 = fourCharacterCode(*b"tex_");

lazy_static! {
  pub static ref TEXTURE_TYPE: Type = Type::object(FRAG_CC, TEXTURE_CC);
  pub static ref TEXTURE_OR_IMAGE_TYPES: Vec<Type> = vec![common_type::image, *TEXTURE_TYPE];
}

pub fn into_vec2(scale_var: &ParamVar) -> Result<egui::Vec2, &'static str> {
  let scale: (f32, f32) = scale_var.get().try_into()?;
  Ok(egui::vec2(scale.0, scale.1))
}

// Resolve image point size based on parameters
pub fn resolve_image_size(
  ui: &egui::Ui,
  size_var: &ParamVar,
  scale_var: &ParamVar,
  aware_var: &ParamVar,
  in_size: egui::Vec2,
) -> egui::Vec2 {
  let mut pt_size = if let Ok(explicit_size) = into_vec2(size_var) {
    explicit_size
  } else {
    in_size
  };

  let scale = into_vec2(scale_var).unwrap_or(vec2(1.0, 1.0));
  // scale cannot be negative, so clamp to 0.0
  let scale = vec2(scale.x.max(0.0), scale.y.max(0.0));
  pt_size = pt_size * scale;
  let scaling_aware: bool = (aware_var.get().try_into()).unwrap_or(false);
  if scaling_aware {
    // Undo UI scaling so the image point size will render it at 1:1 pixel to point ratio
    pt_size / ui.ctx().pixels_per_point()
  } else {
    pt_size
  }
}

pub struct CachedUIImage {
  texture_handle: Option<egui::TextureHandle>,
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

impl CachedUIImage {
  pub fn invalidate(&mut self) {
    self.prev_ptr = null_mut();
  }

  pub fn get_egui_texture_from_image<'a>(
    &'a mut self,
    input: &Var,
    ui: &egui::Ui,
  ) -> Result<&'a egui::TextureHandle, &'static str> {
    let shimage: &SHImage = input.try_into()?;
    let ptr = shimage.data;
    Ok(if ptr != self.prev_ptr {
      let image: egui::ColorImage = into_egui_image(shimage);
      self.prev_ptr = ptr;
      self.texture_handle.insert(ui.ctx().load_texture(
        format!("UI.Image: {:p}", shimage.data),
        image,
        Default::default(),
      ))
    } else {
      self.texture_handle.as_ref().unwrap()
    })
  }
}

pub struct AutoTexturePtr(pub GenericSharedPtr);

impl AutoTexturePtr {
  pub fn new(ptr: *mut gfx_TexturePtr) -> Self {
    let mut sp = GenericSharedPtr::default();
    unsafe {
      gfx_TexturePtr_refAt(&mut sp as *mut _, ptr);
    }
    Self(sp)
  }
}

impl Drop for AutoTexturePtr {
  fn drop(&mut self) {
    unsafe {
      gfx_TexturePtr_unrefAt(&mut self.0 as *mut _);
    }
  }
}

pub fn get_egui_texture_from_gfx(
  ctx: &mut crate::Context,
  input: &Var,
) -> Result<(egui::TextureId, egui::Vec2), &'static str> {
  let texture_ptr: *mut gfx_TexturePtr =
    Var::from_object_ptr_mut_ref::<gfx_TexturePtr>(input, &TEXTURE_TYPE)?;
  if texture_ptr.is_null() {
    return Err("Invalid texture pointer");
  }

  ctx.step_textures.push(AutoTexturePtr::new(texture_ptr));

  let texture_size = {
    let mut texture_res = linalg_aliases_int2::default();
    unsafe { gfx_TexturePtr_getResolution_ext(texture_ptr, &mut texture_res) };
    egui::vec2(texture_res.x as f32, texture_res.y as f32)
  };

  Ok((
    egui::epaint::TextureId::User(texture_ptr as u64),
    texture_size,
  ))
}

fn into_egui_image(image: &SHImage) -> egui::ColorImage {
  assert_eq!(image.channels, 4);

  let size = [image.width as _, image.height as _];
  let rgba = unsafe {
    from_raw_parts_allow_null(
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
    egui::ColorImage { size, pixels }
  } else {
    egui::ColorImage::from_rgba_unmultiplied(size, rgba)
  }
}
