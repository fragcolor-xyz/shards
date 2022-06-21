/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

use super::Window;
use crate::shard::Shard;
use crate::shards::gui::CONTEXT_NAME;
use crate::shards::gui::EGUI_CTX_TYPE;
use crate::shards::gui::EGUI_UI_TYPE;
use crate::shards::gui::PARENT_UI_NAME;
use crate::types::Context;
use crate::types::ExposedInfo;
use crate::types::ExposedTypes;
use crate::types::InstanceData;
use crate::types::ParamVar;
use crate::types::Parameters;
use crate::types::RawString;
use crate::types::ShardsVar;
use crate::types::Type;
use crate::types::Types;
use crate::types::Var;
use crate::types::WireState;
use crate::types::NONE_TYPES;
use crate::types::SHARDS_OR_NONE_TYPES;
use crate::types::STRING_TYPES;
use egui::Context as EguiNativeContext;
use std::rc::Rc;

lazy_static! {
  static ref WINDOW_PARAMETERS: Parameters = vec![
    (
      cstr!("Title"),
      cstr!("The window title displayed on the titlebar."),
      &STRING_TYPES[..],
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
    let mut ctx = ParamVar::new(().into());
    ctx.set_name(CONTEXT_NAME);
    let mut ui_ctx = ParamVar::new(().into());
    ui_ctx.set_name(PARENT_UI_NAME);
    Self {
      instance: ctx,
      requiring: Vec::new(),
      title: ParamVar::new("My Window".into()),
      contents: ShardsVar::default(),
      ui_ctx_instance: ui_ctx,
      ui_ctx_rc: Rc::new(None),
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

  fn inputTypes(&mut self) -> &Types {
    &NONE_TYPES
  }

  fn outputTypes(&mut self) -> &Types {
    &NONE_TYPES
  }

  fn parameters(&mut self) -> Option<&Parameters> {
    Some(&WINDOW_PARAMETERS)
  }

  fn setParam(&mut self, index: i32, value: &Var) -> Result<(), &str> {
    match index {
      0 => Ok(self.title.set_param(value)),
      1 => self.contents.set_param(value),
      _ => Err("Invalid parameter index"),
    }
  }

  fn getParam(&mut self, index: i32) -> Var {
    match index {
      0 => self.title.get_param(),
      1 => self.contents.get_param(),
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

  fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
    // we need to inject UI variable to the inner shards
    let mut data = *data;
    // clone shared into a new vector we can append things to
    let mut shared: ExposedTypes = data.shared.into();
    // append to shared ui vars
    let ui_info = ExposedInfo {
      exposedType: EGUI_UI_TYPE,
      name: self.ui_ctx_instance.get_name(),
      help: cstr!("The parent UI object.").into(),
      isMutable: false,
      isProtected: true, // don't allow to be used in code/wires
      isTableEntry: false,
      global: false,
    };
    shared.push(ui_info);
    // update shared
    data.shared = (&shared).into();

    if !self.contents.is_empty() {
      self.contents.compose(&data)?;
    }

    Ok(data.inputType)
  }

  fn warmup(&mut self, ctx: &Context) -> Result<(), &str> {
    self.instance.warmup(ctx);
    self.ui_ctx_instance.warmup(ctx);

    self.title.warmup(ctx);
    if !self.contents.is_empty() {
      self.contents.warmup(ctx)?;
    }

    Ok(())
  }

  fn cleanup(&mut self) -> Result<(), &str> {
    if !self.contents.is_empty() {
      self.contents.cleanup();
    }
    self.title.cleanup();

    self.ui_ctx_instance.cleanup();
    self.instance.cleanup();

    Ok(())
  }

  fn activate(&mut self, context: &Context, input: &Var) -> Result<Var, &str> {
    let gui_ctx = {
      let ctx_ptr: &mut EguiNativeContext =
        Var::from_object_ptr_mut_ref(self.instance.get(), &EGUI_CTX_TYPE)?;
      &*ctx_ptr
    };

    let mut failed = false;
    if !self.contents.is_empty() {
      let title: &str = self.title.get().as_ref().try_into()?;
      egui::Window::new(title).show(gui_ctx, |ui| {
        // pass the ui parent to the inner shards
        unsafe {
          let var = Var::new_object_from_ptr(ui as *const _, &EGUI_UI_TYPE);
          self.ui_ctx_instance.set(var);
        }

        let mut output = Var::default();
        if self.contents.activate(context, input, &mut output) == WireState::Error {
          failed = true;
          return;
        }
      });

      if failed {
        // TODO add a parameter where we can set to allow some panels to fail!
        return Err("Failed to activate top panel");
      }
    }

    Ok(*input)
  }
}
