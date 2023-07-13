/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

use super::image_util;
use super::ImageButton;
use crate::util;
use crate::FLOAT2_VAR_SLICE;
use crate::PARENTS_UI_NAME;
use shards::shard::Shard;
use shards::shardsc::SHType_Bool;
use shards::shardsc::SHType_Image;
use shards::shardsc::SHType_Object;
use shards::types::common_type;
use shards::types::Context;
use shards::types::ExposedInfo;
use shards::types::ExposedTypes;
use shards::types::InstanceData;
use shards::types::OptionalString;
use shards::types::ParamVar;
use shards::types::Parameters;
use shards::types::ShardsVar;
use shards::types::Type;
use shards::types::Types;
use shards::types::Var;
use shards::types::WireState;
use shards::types::BOOL_TYPES;
use shards::types::BOOL_VAR_OR_NONE_SLICE;
use shards::types::SHARDS_OR_NONE_TYPES;
use std::cmp::Ordering;
use std::ffi::CStr;

lazy_static! {
  static ref IMAGEBUTTON_PARAMETERS: Parameters = vec![
    (
      cstr!("Action"),
      shccstr!("The shards to execute when the button is pressed."),
      &SHARDS_OR_NONE_TYPES[..],
    )
      .into(),
    (
      cstr!("Scale"),
      shccstr!("Scaling to apply to the source image."),
      FLOAT2_VAR_SLICE,
    )
      .into(),
    (
      cstr!("Selected"),
      shccstr!("Indicates whether the button is selected."),
      BOOL_VAR_OR_NONE_SLICE,
    )
      .into(),
  ];
}

impl Default for ImageButton {
  fn default() -> Self {
    let mut parents = ParamVar::default();
    parents.set_name(PARENTS_UI_NAME);
    Self {
      parents,
      requiring: Vec::new(),
      action: ShardsVar::default(),
      scale: ParamVar::new((1.0, 1.0).into()),
      selected: ParamVar::default(),
      exposing: Vec::new(),
      should_expose: false,
      cached_ui_image: Default::default(),
    }
  }
}

impl Shard for ImageButton {
  fn registerName() -> &'static str
  where
    Self: Sized,
  {
    cstr!("UI.ImageButton")
  }

  fn hash() -> u32
  where
    Self: Sized,
  {
    compile_time_crc32::crc32!("UI.ImageButton-rust-0x20200101")
  }

  fn name(&mut self) -> &str {
    "UI.ImageButton"
  }

  fn help(&mut self) -> OptionalString {
    OptionalString(shccstr!("Clickable button with image."))
  }

  fn inputTypes(&mut self) -> &Types {
    &image_util::TEXTURE_OR_IMAGE_TYPES
  }

  fn inputHelp(&mut self) -> OptionalString {
    OptionalString(shccstr!(
      "The value that will be passed to the Action shards of the button."
    ))
  }

  fn outputTypes(&mut self) -> &Types {
    &BOOL_TYPES
  }

  fn outputHelp(&mut self) -> OptionalString {
    OptionalString(shccstr!(
      "Indicates whether the button was clicked during this frame."
    ))
  }

  fn parameters(&mut self) -> Option<&Parameters> {
    Some(&IMAGEBUTTON_PARAMETERS)
  }

  fn setParam(&mut self, index: i32, value: &Var) -> Result<(), &str> {
    match index {
      0 => self.action.set_param(value),
      1 => Ok(self.scale.set_param(value)),
      2 => Ok(self.selected.set_param(value)),
      _ => Err("Invalid parameter index"),
    }
  }

  fn getParam(&mut self, index: i32) -> Var {
    match index {
      0 => self.action.get_param(),
      1 => self.scale.get_param(),
      2 => self.selected.get_param(),
      _ => Var::default(),
    }
  }

  fn requiredVariables(&mut self) -> Option<&ExposedTypes> {
    self.requiring.clear();

    // Add UI.Parents to the list of required variables
    util::require_parents(&mut self.requiring, &self.parents);

    Some(&self.requiring)
  }

  fn exposedVariables(&mut self) -> Option<&ExposedTypes> {
    if self.selected.is_variable() && self.should_expose {
      self.exposing.clear();

      let exp_info = ExposedInfo {
        exposedType: common_type::bool,
        name: self.selected.get_name(),
        help: cstr!("The exposed bool variable").into(),
        ..ExposedInfo::default()
      };

      self.exposing.push(exp_info);
      Some(&self.exposing)
    } else {
      None
    }
  }

  fn hasCompose() -> bool {
    true
  }

  fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
    if self.selected.is_variable() {
      self.should_expose = true; // assume we expose a new variable

      let shared: ExposedTypes = data.shared.into();
      for var in shared {
        let (a, b) = unsafe {
          (
            CStr::from_ptr(var.name),
            CStr::from_ptr(self.selected.get_name()),
          )
        };
        if CStr::cmp(a, b) == Ordering::Equal {
          self.should_expose = false;
          if var.exposedType.basicType != SHType_Bool {
            return Err("ImageButton: bool variable required.");
          }
          break;
        }
      }
    }

    if !self.action.is_empty() {
      self.action.compose(data)?;
    }

    match data.inputType.basicType {
      SHType_Image => decl_override_activate! {
        data.activate = ImageButton::image_activate;
      },
      SHType_Object
        if unsafe { data.inputType.details.object.typeId } == image_util::TEXTURE_CC =>
      {
        decl_override_activate! {
          data.activate = ImageButton::texture_activate;
        }
      }
      _ => (),
    }
    Ok(common_type::bool)
  }

  fn warmup(&mut self, ctx: &Context) -> Result<(), &str> {
    self.parents.warmup(ctx);

    if !self.action.is_empty() {
      self.action.warmup(ctx)?;
    }
    self.scale.warmup(ctx);
    self.selected.warmup(ctx);

    if self.should_expose {
      self.selected.get_mut().valueType = common_type::bool.basicType;
    }

    Ok(())
  }

  fn cleanup(&mut self) -> Result<(), &str> {
    self.selected.cleanup();
    self.scale.cleanup();
    if !self.action.is_empty() {
      self.action.cleanup();
    }

    self.parents.cleanup();

    Ok(())
  }

  fn activate(&mut self, _context: &Context, _input: &Var) -> Result<Var, &str> {
    Err("Invalid input type")
  }
}

impl ImageButton {
  fn activateImage(&mut self, context: &Context, input: &Var) -> Result<Var, &str> {
    if let Some(ui) = util::get_current_parent(self.parents.get())? {
      let (texture_id, texture_size) = {
        let texture = self
          .cached_ui_image
          .get_egui_texture_from_image(input, ui)?;
        let scale = image_util::get_scale(&self.scale)?;
        (texture.into(), texture.size_vec2() * scale)
      };

      self.activateCommon(context, input, ui, texture_id, texture_size)
    } else {
      Err("No UI parent")
    }
  }

  fn activateTexture(&mut self, context: &Context, input: &Var) -> Result<Var, &str> {
    if let Some(ui) = util::get_current_parent(self.parents.get())? {
      let (texture_id, texture_size) = image_util::get_egui_texture_from_gfx(input)?;
      let scale = image_util::get_scale(&self.scale)?;

      self.activateCommon(context, input, ui, texture_id, texture_size * scale)
    } else {
      Err("No UI parent")
    }
  }

  fn activateCommon(
    &mut self,
    context: &Context,
    input: &Var,
    ui: &mut egui::Ui,
    texture_id: egui::TextureId,
    size: egui::Vec2,
  ) -> Result<Var, &str> {
    let mut button = egui::ImageButton::new(texture_id, size);

    let selected = self.selected.get();
    if !selected.is_none() {
      button = button.selected(selected.try_into()?);
    }

    let response = ui.add(button);
    if response.clicked() {
      if self.selected.is_variable() {
        let selected: bool = selected.try_into()?;
        self.selected.set_fast_unsafe(&(!selected).into());
      }
      let mut output = Var::default();
      if self.action.activate(context, input, &mut output) == WireState::Error {
        return Err("Failed to activate button");
      }

      // button clicked during this frame
      return Ok(true.into());
    }

    // button not clicked during this frame
    Ok(false.into())
  }

  impl_override_activate! {
    extern "C" fn image_activate() -> Var {
      ImageButton::activateImage()
    }
  }

  impl_override_activate! {
    extern "C" fn texture_activate() -> Var {
      ImageButton::activateTexture()
    }
  }
}
