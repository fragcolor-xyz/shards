/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

use super::Window;
use super::WindowFlags;
use crate::ANCHOR_TYPES;
use crate::Anchor;
use crate::containers::SEQ_OF_WINDOW_FLAGS;
use crate::containers::WINDOW_FLAGS_TYPE;
use crate::util;
use crate::BOOL_VAR_SLICE;
use crate::CONTEXTS_NAME;
use crate::FLOAT2_VAR_SLICE;
use crate::HELP_OUTPUT_EQUAL_INPUT;
use crate::PARENTS_UI_NAME;
use shards::shard::LegacyShard;
use shards::types::common_type;
use shards::types::Context;
use shards::types::ExposedInfo;
use shards::types::ExposedTypes;
use shards::types::InstanceData;
use shards::types::OptionalString;
use shards::types::ParamVar;
use shards::types::Parameters;
use shards::types::BOOL_OR_NONE_SLICE;

use shards::types::Seq;
use shards::types::ShardsVar;
use shards::types::Type;
use shards::types::Types;
use shards::types::Var;
use shards::types::ANY_TYPES;
use shards::types::INT_OR_NONE_TYPES_SLICE;
use shards::types::SHARDS_OR_NONE_TYPES;
use shards::types::STRING_TYPES;
use shards::types::STRING_VAR_OR_NONE_SLICE;

lazy_static! {
  static ref WINDOW_FLAGS_OR_SEQ_TYPES: Vec<Type> = vec![*WINDOW_FLAGS_TYPE, *SEQ_OF_WINDOW_FLAGS];
  static ref WINDOW_PARAMETERS: Parameters = vec![
    (
      cstr!("Title"),
      shccstr!("The window title displayed on the titlebar."),
      &STRING_TYPES[..],
    )
      .into(),
    (
      cstr!("Position"),
      shccstr!("Absolute position; or when anchor is set, relative offset."),
      FLOAT2_VAR_SLICE,
    )
      .into(),
    (
      cstr!("Anchor"),
      shccstr!("Corner or center of the screen."),
      &ANCHOR_TYPES[..],
    )
      .into(),
    (
      cstr!("Width"),
      shccstr!("The width of the rendered window."),
      INT_OR_NONE_TYPES_SLICE,
    )
      .into(),
    (
      cstr!("Height"),
      shccstr!("The height of the rendered window."),
      INT_OR_NONE_TYPES_SLICE,
    )
      .into(),
    (
      cstr!("Closable"),
      shccstr!("Whether the window will have a close button in its window title bar."),
      BOOL_OR_NONE_SLICE,
    )
      .into(),
    (
      cstr!("Open"),
      shccstr!("Whether the window is open and being shown, or hidden."),
      BOOL_VAR_SLICE,
    )
      .into(),
    (
      cstr!("Flags"),
      shccstr!("Window flags."),
      &WINDOW_FLAGS_OR_SEQ_TYPES[..],
    )
      .into(),
    (
      cstr!("Contents"),
      shccstr!("The UI contents."),
      &SHARDS_OR_NONE_TYPES[..],
    )
      .into(),
    (
      cstr!("ID"),
      shccstr!("An optional ID value to make the window unique if the title name collides."),
      &STRING_VAR_OR_NONE_SLICE[..],
    )
      .into(),
  ];
}

impl Default for Window {
  fn default() -> Self {
    let mut ctx = ParamVar::default();
    ctx.set_name(CONTEXTS_NAME);
    let mut parents = ParamVar::default();
    parents.set_name(PARENTS_UI_NAME);
    Self {
      instance: ctx,
      requiring: Vec::new(),
      title: ParamVar::new(Var::ephemeral_string("My Window")),
      position: ParamVar::default(),
      anchor: ParamVar::default(),
      width: ParamVar::default(),
      height: ParamVar::default(),
      closable: ParamVar::default(),
      open: ParamVar::default(),
      flags: ParamVar::default(),
      contents: ShardsVar::default(),
      parents,
      id: ParamVar::default(),
      cached_id: None,
    }
  }
}

impl LegacyShard for Window {
  fn registerName() -> &'static str
  where
    Self: Sized,
  {
    cstr!("UI.Window")
  }

  fn hash() -> u32
  where
    Self: Sized,
  {
    compile_time_crc32::crc32!("UI.Window-rust-0x20200101")
  }

  fn name(&mut self) -> &str {
    "UI.Window"
  }

  fn help(&mut self) -> OptionalString {
    OptionalString(shccstr!(
      "Creates a floating window which can be dragged, closed, collapsed, and resized."
    ))
  }

  fn inputTypes(&mut self) -> &Types {
    &ANY_TYPES
  }

  fn inputHelp(&mut self) -> OptionalString {
    OptionalString(shccstr!(
      "The value that will be passed to the Contents shards of the rendered window."
    ))
  }

  fn outputTypes(&mut self) -> &Types {
    &ANY_TYPES
  }

  fn outputHelp(&mut self) -> OptionalString {
    *HELP_OUTPUT_EQUAL_INPUT
  }

  fn parameters(&mut self) -> Option<&Parameters> {
    Some(&WINDOW_PARAMETERS)
  }

  fn setParam(&mut self, index: i32, value: &Var) -> Result<(), &str> {
    match index {
      0 => self.title.set_param(value),
      1 => self.position.set_param(value),
      2 => self.anchor.set_param(value),
      3 => self.width.set_param(value),
      4 => self.height.set_param(value),
      5 => self.closable.set_param(value),
      6 => self.open.set_param(value),
      7 => self.flags.set_param(value),
      8 => self.contents.set_param(value),
      9 => self.id.set_param(value),
      _ => Err("Invalid parameter index"),
    }
  }

  fn getParam(&mut self, index: i32) -> Var {
    match index {
      0 => self.title.get_param(),
      1 => self.position.get_param(),
      2 => self.anchor.get_param(),
      3 => self.width.get_param(),
      4 => self.height.get_param(),
      5 => self.closable.get_param(),
      6 => self.open.get_param(),
      7 => self.flags.get_param(),
      8 => self.contents.get_param(),
      9 => self.id.get_param(),
      _ => Var::default(),
    }
  }

  fn requiredVariables(&mut self) -> Option<&ExposedTypes> {
    self.requiring.clear();

    util::require_context(&mut self.requiring);
    util::require_parents(&mut self.requiring);

    if self.id.is_variable() {
      let id_info = ExposedInfo {
        exposedType: common_type::string,
        name: self.id.get_name(),
        help: cstr!("The ID variable.").into(),
        ..ExposedInfo::default()
      };
      self.requiring.push(id_info);
    }

    Some(&self.requiring)
  }

  fn hasCompose() -> bool {
    true
  }

  fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
    if !self.contents.is_empty() {
      self.contents.compose(&data)?;
    }

    // Always passthrough the input
    Ok(data.inputType)
  }

  fn warmup(&mut self, ctx: &Context) -> Result<(), &str> {
    self.instance.warmup(ctx);
    self.parents.warmup(ctx);

    self.title.warmup(ctx);
    self.position.warmup(ctx);
    self.anchor.warmup(ctx);
    self.width.warmup(ctx);
    self.height.warmup(ctx);
    self.closable.warmup(ctx);
    self.open.warmup(ctx);
    self.flags.warmup(ctx);
    if !self.contents.is_empty() {
      self.contents.warmup(ctx)?;
    }
    self.id.warmup(ctx);

    self.cached_id = None;

    Ok(())
  }

  fn cleanup(&mut self) -> Result<(), &str> {
    if !self.contents.is_empty() {
      self.contents.cleanup();
    }
    self.flags.cleanup();
    self.open.cleanup();
    self.closable.cleanup();
    self.height.cleanup();
    self.width.cleanup();
    self.anchor.cleanup();
    self.position.cleanup();
    self.title.cleanup();

    self.parents.cleanup();
    self.instance.cleanup();
    self.id.cleanup();

    Ok(())
  }

  fn activate(&mut self, context: &Context, input: &Var) -> Result<Var, &str> {
    let gui_ctx = &util::get_current_context(&self.instance)?.egui_ctx;

    let mut failed = false;
    if !self.contents.is_empty() {
      let title: &str = self.title.get().try_into()?;
      let mut window = egui::Window::new(title);

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
        window.anchor(anchor.into(),
          offset,
        )
      };

      let width = self.width.get();
      if !width.is_none() {
        let width: i64 = width.try_into()?;
        window = window.min_width(width as f32).default_width(width as f32);
      }

      let height = self.height.get();
      if !height.is_none() {
        let height: i64 = height.try_into()?;
        window = window
          .min_height(height as f32)
          .default_height(height as f32)
      }

      let closable = self.closable.get();
      if !closable.is_none() {
        let closable: bool = closable.try_into()?;

        if closable {
          // if close button is enabled, it must be provided with a state variable for tracking whether window is open
          // this open variable is to provide a way for other Shards to be notified of the window being closed
          if !self.open.is_variable() {
            return Err("Window: open variable required when closable is true");
          } else {
            window = window.open(self.open.get_mut().try_into()?);
          }
        }
      } // else, if no closable provided, then default to false, which does nothing

      for bits in Window::try_get_flags(self.flags.get())? {
        match (WindowFlags { bits }) {
          WindowFlags::NoTitleBar => {
            window = window.title_bar(false);
          }
          WindowFlags::NoResize => {
            window = window.resizable(false);
          }
          WindowFlags::Scrollbars => {
            window = window.scroll2([true, true]);
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

      window.show(gui_ctx, |ui| {
        if !width.is_none() {
          ui.set_width(ui.available_width());
        }
        if !height.is_none() {
          ui.set_height(ui.available_height());
        }
        if util::activate_ui_contents(context, input, ui, &mut self.parents, &mut self.contents)
          .is_err()
        {
          failed = true;
        }
      });

      if failed {
        return Err("Failed to activate window contents");
      }
    }

    // Always passthrough the input
    Ok(*input)
  }
}

impl Window {
  fn try_get_flags(var: &Var) -> Result<Vec<i32>, &str> {
    match var.valueType {
      crate::shardsc::SHType_Enum => Ok(vec![unsafe {
        var.payload.__bindgen_anon_1.__bindgen_anon_3.enumValue
      }]),
      crate::shardsc::SHType_Seq => {
        let seq: Seq = var.try_into()?;
        seq
          .iter()
          .map(|v| Ok(unsafe { v.payload.__bindgen_anon_1.__bindgen_anon_3.enumValue }))
          .collect()
      }
      crate::shardsc::SHType_None => Ok(Vec::new()),
      _ => Err("Invalid type"),
    }
  }
}
