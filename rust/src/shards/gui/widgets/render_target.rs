/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

use std::borrow::BorrowMut;

use egui::Sense;
use egui::Ui;
use egui::Vec2;

use super::RenderTarget;
use crate::fourCharacterCode;
use crate::shard::Shard;
use crate::shards::gui::util;
use crate::shards::gui::widgets::FLOAT2_VAR_SLICE;
use crate::shards::gui::PARENTS_UI_NAME;
use crate::shardsc::gfx_TexturePtr;
use crate::shardsc::gfx_TexturePtr_getResolution_ext;
use crate::shardsc::linalg_aliases_int2;
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

const TextureCC: i32 = fourCharacterCode(*b"tex_");

lazy_static! {
  static ref TEXTURE_TYPE: Type = Type::object(FRAG_CC, TextureCC);
  static ref TEXTURE_TYPES: Vec<Type> = vec![*TEXTURE_TYPE];
  static ref RENDER_TARGET_PARAMETERS: Parameters = vec![(
    cstr!("Scale"),
    cstr!("Scaling to apply to the source texture"),
    FLOAT2_VAR_SLICE,
  )
    .into(),];
}

impl Default for RenderTarget {
  fn default() -> Self {
    let mut parents = ParamVar::default();
    parents.set_name(PARENTS_UI_NAME);
    Self {
      parents,
      requiring: Vec::new(),
      scale: ParamVar::new((1.0, 1.0).into()),
      texture: None,
      prev_ptr: std::ptr::null_mut(),
    }
  }
}

impl Shard for RenderTarget {
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
      "Display the contents of a render target. Consumes input on the region"
    ))
  }

  fn inputTypes(&mut self) -> &Types {
    &TEXTURE_TYPES
  }

  fn inputHelp(&mut self) -> OptionalString {
    OptionalString(shccstr!("The image to display"))
  }

  fn outputTypes(&mut self) -> &Types {
    &TEXTURE_TYPES
  }

  fn outputHelp(&mut self) -> OptionalString {
    OptionalString(shccstr!("The output of this shard will be its input."))
  }

  fn parameters(&mut self) -> Option<&Parameters> {
    Some(&RENDER_TARGET_PARAMETERS)
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

  fn hasCompose() -> bool {
    true
  }

  fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
    match data.inputType.basicType {
      SHType_Object if unsafe { data.inputType.details.object.typeId } == TextureCC => {
        decl_override_activate! {
          data.activate = RenderTarget::texture_activate;
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

impl RenderTarget {
  fn get_scale(scale_var: &ParamVar, ui: &Ui) -> Result<egui::Vec2, &'static str> {
    let scale: (f32, f32) = scale_var.get().try_into()?;
    Ok(egui::vec2(scale.0, scale.1) / ui.ctx().pixels_per_point())
  }

  fn activateTexture(&mut self, _context: &Context, input: &Var) -> Result<Var, &str> {
    if let Some(ui) = util::get_current_parent(*self.parents.get())? {
      let ptr = unsafe { input.payload.__bindgen_anon_1.__bindgen_anon_1.objectValue as u64 };

      let texture_ptr = Var::from_object_ptr_mut_ref::<gfx_TexturePtr>(*input, &TEXTURE_TYPE)?;
      let texture_size = {
        let texture_res = unsafe { gfx_TexturePtr_getResolution_ext(texture_ptr) };
        egui::vec2(texture_res.x as f32, texture_res.y as f32)
      };

      let target_pos = ui.next_widget_position().to_vec2();

      // Manually allocate region to consume input events
      let scale = Self::get_scale(&self.scale, ui)?;
      let (rect, _response) = ui.allocate_exact_size(texture_size * scale, Sense::click_and_drag());

      // Draw texture at this rectangle
      let textureId = egui::epaint::TextureId::User(ptr);
      let image = egui::widgets::Image::new(textureId, rect.size());
      image.paint_at(ui, rect);

      Ok(*input)
    } else {
      Err("No UI parent")
    }
  }

  impl_override_activate! {
    extern "C" fn texture_activate() -> Var {
      RenderTarget::activateTexture()
    }
  }
}
