/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

use std::cmp::Ordering;
use std::ffi::CStr;

use super::PopupLocation;
use super::POPUPLOCATION_TYPES;
use crate::util;
use crate::widgets::image_util;
use crate::FLOAT2_VAR_SLICE;
use shards::cstr;
use shards::shard;
use shards::shard::Shard;
use shards::shard_impl;
use shards::shardsc::SHType_Bool;
use shards::shardsc::SHType_Image;
use shards::shardsc::SHType_Object;

use crate::{CONTEXTS_NAME, PARENTS_UI_NAME};
use shards::{
  core::register_shard,
  types::{
    common_type, Context, ExposedInfo, ExposedTypes, InstanceData, OptionalString, ParamVar,
    ShardsVar, Type, Types, Var, BOOL_TYPES, BOOL_VAR_OR_NONE_SLICE, FLOAT_OR_NONE_TYPES_SLICE,
    SHARDS_OR_NONE_TYPES, STRING_VAR_OR_NONE_SLICE,
  },
};

#[derive(shard)]
#[shard_info(
  "UI.PopupImageButton",
  "Wraps a button with a popup that can act as a drop-down menu or suggestion menu."
)]
struct PopupImageButton {
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
  #[shard_param("MinWidth", "The minimum width of the popup that should appear below or above the button. By default, it is always at least as wide as the button.", FLOAT_OR_NONE_TYPES_SLICE)]
  pub min_width: ParamVar,
  #[shard_param(
    "AboveOrBelow",
    "Whether the location of the popup should be above or below the button.",
    POPUPLOCATION_TYPES
  )]
  pub above_or_below: ParamVar,
  #[shard_param(
    "ID",
    "An optional ID value to make the popup unique if the label text collides.",
    STRING_VAR_OR_NONE_SLICE
  )]
  pub id: ParamVar,
  pub cached_id: Option<egui::Id>,
  #[shard_param(
    "Contents",
    "The shards to execute and render inside the popup ui when the button is pressed.",
    SHARDS_OR_NONE_TYPES
  )]
  pub contents: ShardsVar,
  #[shard_warmup]
  contexts: ParamVar,
  #[shard_warmup]
  parents: ParamVar,
  #[shard_required]
  required: ExposedTypes,
  exposing: ExposedTypes,
  should_expose: bool,
  cached_ui_image: image_util::CachedUIImage,
}

impl Default for PopupImageButton {
  fn default() -> Self {
    Self {
      contexts: ParamVar::new_named(CONTEXTS_NAME),
      parents: ParamVar::new_named(PARENTS_UI_NAME),
      scale: ParamVar::new((1.0, 1.0).into()),
      size: ParamVar::default(),
      scaling_aware: ParamVar::default(),
      selected: ParamVar::default(),
      above_or_below: ParamVar::default(),
      min_width: ParamVar::default(),
      id: ParamVar::default(),
      cached_id: None,
      contents: ShardsVar::default(),
      required: Vec::new(),
      exposing: Vec::new(),
      should_expose: false,
      cached_ui_image: Default::default(),
    }
  }
}

#[shard_impl]
impl Shard for PopupImageButton {
  fn input_types(&mut self) -> &Types {
    &image_util::TEXTURE_OR_IMAGE_TYPES
  }

  fn input_help(&mut self) -> OptionalString {
    OptionalString(shccstr!(
      "The value that will be passed to the Contents shards of the popup image button."
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
        help: cstr!("The exposed bool variable").into(),
        ..ExposedInfo::default()
      };

      self.exposing.push(exp_info);
      Some(&self.exposing)
    } else {
      None
    }
  }

  fn warmup(&mut self, context: &Context) -> Result<(), &str> {
    self.warmup_helper(context)?;

    Ok(())
  }

  fn cleanup(&mut self) -> Result<(), &str> {
    self.cleanup_helper()?;

    Ok(())
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
            return Err("PopupImageButton: bool variable required.");
          }
          break;
        }
      }
    }

    if self.id.is_variable() {
      let id_info = ExposedInfo {
        exposedType: common_type::string,
        name: self.id.get_name(),
        help: cstr!("The ID variable.").into(),
        ..ExposedInfo::default()
      };
      self.required.push(id_info);
    }

    if !self.contents.is_empty() {
      self.contents.compose(data)?;
    }

    match data.inputType.basicType {
      SHType_Image => decl_override_activate! {
        data.activate = PopupImageButton::image_activate;
      },
      SHType_Object
        if unsafe { data.inputType.details.object.typeId } == image_util::TEXTURE_CC =>
      {
        decl_override_activate! {
          data.activate = PopupImageButton::texture_activate;
        }
      }
      _ => (),
    }
    Ok(common_type::bool)
  }

  fn activate(&mut self, context: &Context, input: &Var) -> Result<Var, &str> {
    Err("Invalid input type")
  }
}

impl PopupImageButton {
  fn activate_image(&mut self, context: &Context, input: &Var) -> Result<Var, &str> {
    let ui = util::get_parent_ui(self.parents.get())?;
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

    self.activate_common(context, input, ui, texture_id, texture_size)
  }

  fn activate_texture(&mut self, context: &Context, input: &Var) -> Result<Var, &str> {
    let ui = util::get_parent_ui(self.parents.get())?;
    let (texture_id, texture_size) = image_util::get_egui_texture_from_gfx(input)?;
    self.activate_common(
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
    context: &Context,
    input: &Var,
    ui: &mut egui::Ui,
    texture_id: egui::TextureId,
    size: egui::Vec2,
  ) -> Result<Var, &str> {
    if let Some(ui) = util::get_current_parent_opt(self.parents.get())? {
      let mut button = egui::ImageButton::new(texture_id, size);

      let selected = self.selected.get();
      if !selected.is_none() {
        button = button.selected(selected.try_into()?);
      }

      let response = ui.add(button);
      let popup_id = if let Ok(id) = <&str>::try_from(self.id.get()) {
        self
          .cached_id
          .get_or_insert_with(|| ui.make_persistent_id(id))
      } else {
        return Err("A PopupImageButton must be provided with a valid ID string.");
      };

      if response.clicked() {
        ui.memory_mut(|mem| mem.toggle_popup(*popup_id));
      }

      let above_or_below = if self.above_or_below.get().is_none() {
        egui::AboveOrBelow::Below
      } else {
        let above_or_below: PopupLocation = self.above_or_below.get().try_into()?;
        above_or_below.into()
      };

      if !self.contents.is_empty() {
        if let Some(inner) =
          egui::popup::popup_above_or_below_widget(ui, *popup_id, &response, above_or_below, |ui| {
            if !self.min_width.get().is_none() {
              ui.set_min_width(self.min_width.get().try_into()?);
            }
            util::activate_ui_contents(context, input, ui, &mut self.parents, &mut self.contents)
          })
        {
          // Only if popup is open will there be an inner result. In such a case, verify that nothing went wrong.
          inner?;
        }
      }

      if response.clicked() {
        if self.selected.is_variable() {
          let selected: bool = selected.try_into()?;
          self.selected.set_fast_unsafe(&(!selected).into());
        }

        // button clicked during this frame
        return Ok(true.into());
      }
    } else {
      return Err("No UI parent");
    }

    // button not clicked during this frame
    Ok(false.into())
  }

  impl_override_activate! {
    extern "C" fn image_activate() -> Var {
      PopupImageButton::activate_image()
    }
  }

  impl_override_activate! {
    extern "C" fn texture_activate() -> Var {
      PopupImageButton::activate_texture()
    }
  }
}

pub fn register_shards() {
  register_shard::<PopupImageButton>();
}
