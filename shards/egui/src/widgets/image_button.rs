/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

use super::image_util;
use crate::util;
use crate::CONTEXTS_NAME;
use crate::FLOAT2_VAR_SLICE;
use crate::PARENTS_UI_NAME;
use egui::load::SizedTexture;
use shards::core::register_shard;
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

#[derive(shard)]
#[shard_info("UI.ImageButton", "Clickable button with image.")]
struct ImageButton {
  #[shard_param(
    "Action",
    "The shards to execute when the button is pressed.",
    SHARDS_OR_NONE_TYPES
  )]
  action: ShardsVar,
  #[shard_param("Scale", "Scaling to apply to the source image.", FLOAT2_VAR_SLICE)]
  scale: ParamVar,
  #[shard_param("Size", "The size to render the image at.", FLOAT2_VAR_SLICE)]
  size: ParamVar,
  #[shard_param("ScalingAware", "When set to true, this image's pixels will be rendered 1:1 regardless of UI context point size.", BOOL_VAR_OR_NONE_SLICE)]
  scaling_aware: ParamVar,
  #[shard_param(
    "Selected",
    "Indicates whether the button is selected.",
    BOOL_VAR_OR_NONE_SLICE
  )]
  selected: ParamVar,
  exposing: ExposedTypes,
  should_expose: bool,
  cached_ui_image: image_util::CachedUIImage,
  #[shard_warmup]
  contexts: ParamVar,
  #[shard_warmup]
  parents: ParamVar,
  #[shard_required]
  required: ExposedTypes,
}

impl Default for ImageButton {
  fn default() -> Self {
    Self {
      contexts: ParamVar::new_named(CONTEXTS_NAME),
      parents: ParamVar::new_named(PARENTS_UI_NAME),
      required: Vec::new(),
      action: ShardsVar::default(),
      scale: ParamVar::new((1.0, 1.0).into()),
      size: ParamVar::default(),
      scaling_aware: ParamVar::default(),
      selected: ParamVar::default(),
      exposing: Vec::new(),
      should_expose: false,
      cached_ui_image: Default::default(),
    }
  }
}

#[shard_impl]
impl Shard for ImageButton {
  fn input_types(&mut self) -> &Types {
    &image_util::TEXTURE_OR_IMAGE_TYPES
  }

  fn input_help(&mut self) -> OptionalString {
    OptionalString(shccstr!(
      "The value that will be passed to the Action shards of the button."
    ))
  }

  fn output_types(&mut self) -> &Types {
    &BOOL_TYPES
  }

  fn output_help(&mut self) -> OptionalString {
    OptionalString(shccstr!(
      "Indicates whether the button was clicked during this frame."
    ))
  }

  fn exposed_variables(&mut self) -> Option<&ExposedTypes> {
    if self.selected.is_variable() && self.should_expose {
      self.exposing.clear();

      let exp_info = ExposedInfo {
        exposedType: common_type::bool,
        name: self.selected.get_name(),
        help: shccstr!("The exposed bool variable"),
        declared: true,
        isMutable: true,
        ..ExposedInfo::default()
      };

      self.exposing.push(exp_info);
      Some(&self.exposing)
    } else {
      None
    }
  }

  fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
    self.compose_helper(data)?;

    util::require_context(&mut self.required);
    util::require_parents(&mut self.required);

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
    self.warmup_helper(ctx)?;

    if self.should_expose {
      self.selected.get_mut().valueType = common_type::bool.basicType;
    }

    Ok(())
  }

  fn cleanup(&mut self, ctx: Option<&Context>) -> Result<(), &str> {
    self.cleanup_helper(ctx)?;

    Ok(())
  }

  fn activate(&mut self, _context: &Context, _input: &Var) -> Result<Option<Var>, &str> {
    Err("Invalid input type")
  }
}

impl ImageButton {
  fn activate_image(&mut self, context: &Context, input: &Var) -> Result<Option<Var>, &str> {
    let ui = util::get_parent_ui(self.parents.get())?;
    let ctx = util::get_current_context(&self.contexts)?;

    let (texture_id, texture_size) = {
      let texture = self
        .cached_ui_image
        .get_egui_texture_from_image(input, ui)?;
      (
        texture.into(),
        image_util::resolve_image_size(
          ui,
          &self.size,
          &self.scale,
          &self.scaling_aware,
          texture.size_vec2(),
        ),
      )
    };

    self.activate_common(ctx, context, input, ui, texture_id, texture_size)
  }

  fn activate_texture(&mut self, context: &Context, input: &Var) -> Result<Option<Var>, &str> {
    let ui = util::get_parent_ui(self.parents.get())?;
    let ctx = util::get_current_context(&self.contexts)?;

    let (texture_id, texture_size) = image_util::get_egui_texture_from_gfx(ctx, input)?;

    self.activate_common(
      ctx,
      context,
      input,
      ui,
      texture_id,
      image_util::resolve_image_size(
        ui,
        &self.size,
        &self.scale,
        &self.scaling_aware,
        texture_size,
      ),
    )
  }

  fn activate_common(
    &mut self,
    egui_ctx: &mut crate::Context,
    context: &Context,
    input: &Var,
    ui: &mut egui::Ui,
    texture_id: egui::TextureId,
    size: egui::Vec2,
  ) -> Result<Option<Var>, &str> {
    let img = SizedTexture {
      id: texture_id,
      size,
    };
    let mut button = egui::ImageButton::new(img);

    let selected = self.selected.get();
    if !selected.is_none() {
      button = button.selected(selected.try_into()?);
    }

    let mut button_clicked = false;

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
      button_clicked = true;
    }

    // Store response in context to support shards like PopupWrapper, which uses a stored response in order to wrap behavior around it
    egui_ctx.prev_response = Some(response);
    if button_clicked {
      egui_ctx.override_selection_response = egui_ctx.prev_response.clone();
    }

    // button not clicked during this frame
    Ok(Some(button_clicked.into()))
  }

  impl_override_activate! {
    extern "C" fn image_activate() -> *const Var {
      ImageButton::activate_image()
    }
  }

  impl_override_activate! {
    extern "C" fn texture_activate() -> *const Var {
      ImageButton::activate_texture()
    }
  }
}

pub fn register_shards() {
  register_shard::<ImageButton>();
}
