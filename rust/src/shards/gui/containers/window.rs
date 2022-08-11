/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

use super::Window;
use crate::shard::Shard;
use crate::shards::gui::util;
use crate::shards::gui::CONTEXT_NAME;
use crate::shards::gui::EGUI_CTX_TYPE;
use crate::shards::gui::EGUI_UI_SEQ_TYPE;
use crate::shards::gui::PARENTS_UI_NAME;
use crate::types::Context;
use crate::types::ExposedInfo;
use crate::types::ExposedTypes;
use crate::types::InstanceData;
use crate::types::OptionalString;
use crate::types::ParamVar;
use crate::types::Parameters;
use crate::types::RawString;
use crate::types::ShardsVar;
use crate::types::Type;
use crate::types::Types;
use crate::types::Var;
use crate::types::ANY_TYPES;
use crate::types::INT2_OR_NONE_TYPES_SLICE;
use crate::types::INT_OR_NONE_TYPES_SLICE;
use crate::types::SHARDS_OR_NONE_TYPES;
use crate::types::STRING_TYPES;
use egui::Context as EguiNativeContext;

lazy_static! {
  static ref WINDOW_PARAMETERS: Parameters = vec![
    (
      cstr!("Title"),
      cstr!("The window title displayed on the titlebar."),
      &STRING_TYPES[..],
    )
      .into(),
    (
      cstr!("Position"),
      cstr!("The (x,y) position of the rendered window."),
      INT2_OR_NONE_TYPES_SLICE,
    )
      .into(),
    (
      cstr!("Width"),
      cstr!("The width of the rendered window."),
      INT_OR_NONE_TYPES_SLICE,
    )
      .into(),
    (
      cstr!("Height"),
      cstr!("The height of the rendered window."),
      INT_OR_NONE_TYPES_SLICE,
    )
      .into(),
    (
      cstr!("Contents"),
      cstr!("The UI contents."),
      &SHARDS_OR_NONE_TYPES[..],
    )
      .into(),
  ];
}

impl Default for Window {
  fn default() -> Self {
    let mut ctx = ParamVar::default();
    ctx.set_name(CONTEXT_NAME);
    let mut parents = ParamVar::default();
    parents.set_name(PARENTS_UI_NAME);
    Self {
      instance: ctx,
      requiring: Vec::new(),
      title: ParamVar::new(Var::ephemeral_string("My Window")),
      position: ParamVar::default(),
      width: ParamVar::default(),
      height: ParamVar::default(),
      contents: ShardsVar::default(),
      parents,
      exposing: Vec::new(),
    }
  }
}

impl Shard for Window {
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
      "Creates a floating window which can be dragged, closed, collapsed, resized."
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
    OptionalString(shccstr!("The output of this shard will be its input."))
  }

  fn parameters(&mut self) -> Option<&Parameters> {
    Some(&WINDOW_PARAMETERS)
  }

  fn setParam(&mut self, index: i32, value: &Var) -> Result<(), &str> {
    match index {
      0 => Ok(self.title.set_param(value)),
      1 => Ok(self.position.set_param(value)),
      2 => Ok(self.width.set_param(value)),
      3 => Ok(self.height.set_param(value)),
      4 => self.contents.set_param(value),
      _ => Err("Invalid parameter index"),
    }
  }

  fn getParam(&mut self, index: i32) -> Var {
    match index {
      0 => self.title.get_param(),
      1 => self.position.get_param(),
      2 => self.width.get_param(),
      3 => self.height.get_param(),
      4 => self.contents.get_param(),
      _ => Var::default(),
    }
  }

  fn requiredVariables(&mut self) -> Option<&ExposedTypes> {
    self.requiring.clear();

    // Add UI.Context to the list of required variables
    let exp_info = ExposedInfo {
      exposedType: EGUI_CTX_TYPE,
      name: self.instance.get_name(),
      help: cstr!("The exposed UI context.").into(),
      ..ExposedInfo::default()
    };
    self.requiring.push(exp_info);

    Some(&self.requiring)
  }

  fn exposedVariables(&mut self) -> Option<&ExposedTypes> {
    self.exposing.clear();

    if util::expose_contents_variables(&mut self.exposing, &self.contents) {
      Some(&self.exposing)
    } else {
      None
    }
  }

  fn hasCompose() -> bool {
    true
  }

  fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
    // we need to inject UI variable to the inner shards
    let mut data = *data;
    // clone shared into a new vector we can append things to
    let mut shared: ExposedTypes = data.shared.into();
    // append to shared ui vars
    let ui_info = ExposedInfo {
      exposedType: EGUI_UI_SEQ_TYPE,
      name: self.parents.get_name(),
      help: cstr!("The parent UI objects.").into(),
      isMutable: false,
      isProtected: true, // don't allow to be used in code/wires
      isTableEntry: false,
      global: false,
      isPushTable: false,
    };
    shared.push(ui_info);
    // update shared
    data.shared = (&shared).into();

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
    self.width.warmup(ctx);
    self.height.warmup(ctx);
    if !self.contents.is_empty() {
      self.contents.warmup(ctx)?;
    }

    Ok(())
  }

  fn cleanup(&mut self) -> Result<(), &str> {
    if !self.contents.is_empty() {
      self.contents.cleanup();
    }
    self.height.cleanup();
    self.width.cleanup();
    self.position.cleanup();
    self.title.cleanup();

    self.parents.cleanup();
    self.instance.cleanup();

    Ok(())
  }

  fn activate(&mut self, context: &Context, input: &Var) -> Result<Var, &str> {
    let gui_ctx = {
      let ctx_ptr: &mut EguiNativeContext =
        Var::from_object_ptr_mut_ref(*self.instance.get(), &EGUI_CTX_TYPE)?;
      &*ctx_ptr
    };

    let mut failed = false;
    if !self.contents.is_empty() {
      let title: &str = self.title.get().try_into()?;
      let mut window = egui::Window::new(title);
      let rect = gui_ctx.available_rect();

      let position = self.position.get();
      if !position.is_none() {
        // note: in egui the position is relative to the top-left corner of the whole UI
        // but a window is constrained by the available rect of the central panel.
        // Thus, we offset it to make it more intuitive for users.
        // i.e. the position is now relative to the top-left corner of the central panel.
        let position: (i64, i64) = position.try_into()?;
        window = window.default_pos(egui::Pos2 {
          x: position.0 as f32 + rect.min.x,
          y: position.1 as f32 + rect.min.y,
        });
      }

      let width = self.width.get();
      if !width.is_none() {
        let width: i64 = width.try_into()?;
        window = window.default_width(width as f32);
      }

      let height = self.height.get();
      if !height.is_none() {
        let height: i64 = height.try_into()?;
        window = window.default_height(height as f32);
      }

      window.show(gui_ctx, |ui| {
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
