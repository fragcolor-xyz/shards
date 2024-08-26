/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

use crate::containers::WindowFlags;
use crate::containers::SEQ_OF_WINDOW_FLAGS;
use crate::containers::WINDOW_FLAGS_TYPE;
use crate::util;
use crate::util::with_possible_panic;
use crate::Anchor;
use crate::ANCHOR_TYPES;
use crate::FLOAT_VAR_OR_NONE_SLICE;
use crate::HELP_OUTPUT_EQUAL_INPUT;
use crate::{CONTEXTS_NAME, FLOAT2_VAR_SLICE, PARENTS_UI_NAME};
use num_traits::clamp;
use shards::shard;
use shards::shard::Shard;
use shards::shard_impl;
use shards::types::ClonedVar;
use shards::types::OptionalString;
use shards::types::WireState;
use shards::types::ANY_TYPES;
use shards::types::{
  common_type, Context, ExposedInfo, ExposedTypes, InstanceData, ParamVar, Seq, ShardsVar, Type,
  Types, Var, SHARDS_OR_NONE_TYPES,
};
use shards::{core::register_shard, types::STRING_VAR_OR_NONE_SLICE};

lazy_static! {
  static ref WINDOW_FLAGS_OR_SEQ_TYPES: Vec<Type> = vec![*WINDOW_FLAGS_TYPE, *SEQ_OF_WINDOW_FLAGS];
}

#[derive(shard)]
#[shard_info(
  "UI.Window",
  "Creates a floating window which can be dragged, closed, collapsed, and resized."
)]
struct WindowShard {
  #[shard_param(
    "Title",
    "The window title displayed on the title bar.",
    STRING_VAR_OR_NONE_SLICE
  )]
  pub title: ParamVar,
  #[shard_param("Contents", "The UI contents.", SHARDS_OR_NONE_TYPES)]
  pub contents: ShardsVar,
  #[shard_param(
    "Position",
    "Absolute position; or when anchor is set, relative offset.",
    FLOAT2_VAR_SLICE
  )]
  pub position: ParamVar,
  #[shard_param("Anchor", "Corner or center of the screen.", ANCHOR_TYPES)]
  pub anchor: ParamVar,
  #[shard_param(
    "MinWidth",
    "The minimum width of the window.",
    FLOAT_VAR_OR_NONE_SLICE
  )]
  pub min_width: ParamVar,
  #[shard_param(
    "MinHeight",
    "The minimum height of the window.",
    FLOAT_VAR_OR_NONE_SLICE
  )]
  pub min_height: ParamVar,
  #[shard_param(
    "MaxWidth",
    "The maximum width of the window.",
    FLOAT_VAR_OR_NONE_SLICE
  )]
  pub max_width: ParamVar,
  #[shard_param(
    "MaxHeight",
    "The maximum height of the window.",
    FLOAT_VAR_OR_NONE_SLICE
  )]
  pub max_height: ParamVar,
  #[shard_param(
    "FixedWidth",
    "The fixed size of the window. overrides all other min/max sizes.",
    FLOAT_VAR_OR_NONE_SLICE
  )]
  pub fixed_width: ParamVar,
  #[shard_param(
    "FixedHeight",
    "The fixed size of the window. overrides all other min/max sizes.",
    FLOAT_VAR_OR_NONE_SLICE
  )]
  pub fixed_height: ParamVar,
  #[shard_param(
    "Closed",
    "When provided with a callback, this window will have a close button and call this when pressed.",
    SHARDS_OR_NONE_TYPES,
  )]
  pub closed: ShardsVar,
  #[shard_param("Flags", "Window flags.", WINDOW_FLAGS_OR_SEQ_TYPES)]
  pub flags: ParamVar,
  #[shard_param(
    "ID",
    "An optional ID value to make the window unique if the title name collides.",
    STRING_VAR_OR_NONE_SLICE
  )]
  pub id: ParamVar,
  #[shard_param("Transparency", "If not None, it sets the window's transparency level based on the alpha value.", [common_type::float, common_type::none])]
  pub transparent: ClonedVar,

  pub cached_id: Option<egui::Id>,

  #[shard_warmup]
  contexts: ParamVar,
  #[shard_warmup]
  parents: ParamVar,
  #[shard_required]
  required: ExposedTypes,
  has_close_button: bool,
}

impl Default for WindowShard {
  fn default() -> Self {
    Self {
      contexts: ParamVar::new_named(CONTEXTS_NAME),
      parents: ParamVar::new_named(PARENTS_UI_NAME),
      title: ParamVar::default(),
      position: ParamVar::default(),
      anchor: ParamVar::default(),
      min_width: ParamVar::default(),
      min_height: ParamVar::default(),
      max_width: ParamVar::default(),
      max_height: ParamVar::default(),
      fixed_width: ParamVar::default(),
      fixed_height: ParamVar::default(),
      closed: ShardsVar::default(),
      flags: ParamVar::default(),
      id: ParamVar::default(),
      cached_id: None,
      contents: ShardsVar::default(),
      required: Vec::new(),
      has_close_button: false,
      transparent: false.into(),
    }
  }
}

#[shard_impl]
impl Shard for WindowShard {
  fn input_types(&mut self) -> &Types {
    &ANY_TYPES
  }

  fn input_help(&mut self) -> OptionalString {
    OptionalString(shccstr!(
      "The value that will be passed to the Contents shards of the rendered window."
    ))
  }

  fn output_types(&mut self) -> &Types {
    &ANY_TYPES
  }

  fn output_help(&mut self) -> OptionalString {
    *HELP_OUTPUT_EQUAL_INPUT
  }

  fn warmup(&mut self, context: &Context) -> Result<(), &str> {
    self.warmup_helper(context)?;

    Ok(())
  }

  fn cleanup(&mut self, ctx: Option<&Context>) -> Result<(), &str> {
    self.cleanup_helper(ctx)?;

    Ok(())
  }

  fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
    self.compose_helper(data)?;

    util::require_context(&mut self.required);
    util::require_parents(&mut self.required);

    if self.id.is_variable() {
      let id_info = ExposedInfo {
        exposedType: common_type::string,
        name: self.id.get_name(),
        help: shccstr!("The ID variable."),
        ..ExposedInfo::default()
      };
      self.required.push(id_info);
    }

    self.contents.compose(data)?;

    self.has_close_button = !self.closed.is_empty();
    if self.has_close_button {
      self.closed.compose(data)?;
    }

    // Always passthrough the input
    Ok(data.inputType)
  }

  fn activate(&mut self, context: &Context, input: &Var) -> Result<Option<Var>, &str> {
    // If the window should be drawn this frame
    let gui_ctx = &util::get_current_context(&self.contexts)?.egui_ctx;

    let mut open = true;
    let mut failed = false;
    if !self.contents.is_empty() {
      let title: &str = self.title.get().try_into().unwrap_or_default();
      let mut window = egui::Window::new(title);

      if title == "" {
        window = window.title_bar(false);
      }

      let transparency: f64 = self.transparent.0.as_ref().try_into().unwrap_or(1.0);
      if transparency < 1.0 {
        window = window.frame(
          egui::Frame::none().fill(egui::Color32::from_rgba_premultiplied(
            0,
            0,
            0,
            (clamp(transparency, 0.0, 1.0) * 255.0) as u8,
          )),
        );
      }

      if let Ok(id) = <&str>::try_from(self.id.get()) {
        let id = self.cached_id.get_or_insert_with(|| egui::Id::new(id));
        window = window.id(*id);
      }

      window = if self.anchor.get().is_none() {
        // note: in egui the position is relative to the top-left corner of the whole UI
        // but a window is constrained by the available rect of the central panel.
        // Thus, we offset it to make it more intuitive for users.
        // i.e. the position is now relative to the top-left corner of the central panel.
        let position = self.position.get();
        if !position.is_none() {
          let pos: (f32, f32) = self.position.get().try_into()?;
          let rect = gui_ctx.available_rect();
          window.default_pos(egui::Pos2 {
            x: pos.0 + rect.min.x,
            y: pos.1 + rect.min.y,
          })
        } else {
          window
        }
      } else {
        let offset: (f32, f32) = self.position.get().try_into().unwrap_or_default();
        let anchor: Anchor = self.anchor.get().try_into()?;
        window.anchor(anchor.into(), offset)
      };

      let min_width: f32 = self.min_width.get().try_into().unwrap_or(-1.0);
      let min_height: f32 = self.min_height.get().try_into().unwrap_or(-1.0);
      let max_width: f32 = self.max_width.get().try_into().unwrap_or(-1.0);
      let max_height: f32 = self.max_height.get().try_into().unwrap_or(-1.0);
      let fixed_width: f32 = self.fixed_width.get().try_into().unwrap_or(-1.0);
      let fixed_height: f32 = self.fixed_height.get().try_into().unwrap_or(-1.0);
      if fixed_width >= 0.0 {
        window = window
          .min_width(fixed_width)
          .max_width(fixed_width)
          .default_width(fixed_width);
      } else {
        if min_width >= 0.0 {
          window = window.min_width(min_width);
        }
        if max_width >= 0.0 {
          window = window.max_width(max_width);
        }
      }
      if fixed_height >= 0.0 {
        window = window
          .min_height(fixed_height)
          .max_height(fixed_height)
          .default_height(fixed_height);
      } else {
        if min_height >= 0.0 {
          window = window.min_height(min_height);
        }
        if max_height >= 0.0 {
          window = window.max_height(max_height);
        }
      }

      if self.has_close_button {
        // if close button is enabled, it must be provided with a state variable for tracking whether window is open
        // this open variable is to provide a way for other Shards to be notified of the window being closed
        window = window.open(&mut open);
      }
      // else, if no closable provided, then default to false, which does nothing

      for bits in WindowShard::try_get_flags(self.flags.get())? {
        match (WindowFlags { bits }) {
          WindowFlags::NoTitleBar => {
            window = window.title_bar(false);
          }
          WindowFlags::NoResize => {
            window = window.resizable(false);
          }
          WindowFlags::Scrollbars => {
            window = window.scroll([true, true]);
          }
          WindowFlags::NoCollapse => {
            window = window.collapsible(false);
          }
          WindowFlags::Immovable => {
            window = window.movable(false);
          }
          _ => (),
        }
      }

      with_possible_panic(|| {
        window.show(gui_ctx, |ui| {
          if util::activate_ui_contents(context, input, ui, &mut self.parents, &mut self.contents)
            .is_err()
          {
            failed = true;
          }
        });
      })?;

      if failed {
        return Err("Failed to activate window contents");
      }

      if !open {
        let mut tmp = Var::default();
        if self.closed.activate(context, input, &mut tmp) == WireState::Error {
          return Err("Failed to activate close callback");
        }
      }
    }

    // Always passthrough the input
    Ok(None)
  }
}

impl WindowShard {
  fn try_get_flags(var: &Var) -> Result<Vec<i32>, &str> {
    match var.valueType {
      shards::SHType_Enum => Ok(vec![unsafe {
        var.payload.__bindgen_anon_1.__bindgen_anon_3.enumValue
      }]),
      shards::SHType_Seq => {
        let seq: Seq = var.try_into()?;
        seq
          .iter()
          .map(|v| Ok(unsafe { v.payload.__bindgen_anon_1.__bindgen_anon_3.enumValue }))
          .collect()
      }
      shards::SHType_None => Ok(Vec::new()),
      _ => Err("Invalid type"),
    }
  }
}

pub fn register_shards() {
  register_shard::<WindowShard>();
}
