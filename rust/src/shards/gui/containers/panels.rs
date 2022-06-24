/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

use super::Panels;
use crate::shard::Shard;
use crate::shards::gui::EguiId;
use crate::shards::gui::CONTEXT_NAME;
use crate::shards::gui::EGUI_CTX_TYPE;
use crate::shards::gui::EGUI_UI_SEQ_TYPE;
use crate::shards::gui::EGUI_UI_TYPE;
use crate::shards::gui::PARENTS_UI_NAME;
use crate::types::Context;
use crate::types::ExposedInfo;
use crate::types::ExposedTypes;
use crate::types::InstanceData;
use crate::types::OptionalString;
use crate::types::ParamVar;
use crate::types::Parameters;
use crate::types::RawString;
use crate::types::Seq;
use crate::types::ShardsVar;
use crate::types::Type;
use crate::types::Var;
use crate::types::WireState;
use crate::types::ANY_TYPES;
use crate::types::SHARDS_OR_NONE_TYPES;
use egui::Context as EguiNativeContext;

lazy_static! {
  static ref PANELS_PARAMETERS: Parameters = vec![
    (
      cstr!("Top"),
      cstr!("A panel that covers the entire top of a UI surface."),
      &SHARDS_OR_NONE_TYPES[..],
    ).into(),
    (
      cstr!("Left"),
      cstr!("A panel that covers the entire left side of a UI surface."),
      &SHARDS_OR_NONE_TYPES[..],
    ).into(),
    (
      cstr!("Center"),
      cstr!("A panel that covers the remainder of the screen, i.e. whatever area is left after adding other panels."),
      &SHARDS_OR_NONE_TYPES[..],
    ).into(),
    (
      cstr!("Right"),
      cstr!("A panel that covers the entire right side of a UI surface."),
      &SHARDS_OR_NONE_TYPES[..],
    ).into(),
    (
      cstr!("Bottom"),
      cstr!("A panel that covers the entire bottom of a UI surface."),
      &SHARDS_OR_NONE_TYPES[..],
    ).into(),
  ];
}

impl Default for Panels {
  fn default() -> Self {
    let mut ctx = ParamVar::new(().into());
    ctx.set_name(CONTEXT_NAME);
    let mut ui_ctx = ParamVar::new(().into());
    ui_ctx.set_name(PARENTS_UI_NAME);
    Self {
      instance: ctx,
      requiring: Vec::new(),
      top: ShardsVar::default(),
      left: ShardsVar::default(),
      center: ShardsVar::default(),
      right: ShardsVar::default(),
      bottom: ShardsVar::default(),
      ui_ctx_instance: ui_ctx,
    }
  }
}

impl Shard for Panels {
  fn registerName() -> &'static str {
    cstr!("UI.Panels")
  }

  fn hash() -> u32 {
    compile_time_crc32::crc32!("UI.Panels-rust-0x20200101")
  }

  fn name(&mut self) -> &str {
    "UI.Panels"
  }

  fn help(&mut self) -> OptionalString {
    OptionalString(shccstr!("Layout UI elements into panels."))
  }

  fn inputTypes(&mut self) -> &std::vec::Vec<Type> {
    &ANY_TYPES
  }

  fn outputTypes(&mut self) -> &std::vec::Vec<Type> {
    &ANY_TYPES
  }

  fn parameters(&mut self) -> Option<&Parameters> {
    Some(&PANELS_PARAMETERS)
  }

  fn setParam(&mut self, index: i32, value: &Var) -> Result<(), &str> {
    match index {
      0 => self.top.set_param(value),
      1 => self.left.set_param(value),
      2 => self.center.set_param(value),
      3 => self.right.set_param(value),
      4 => self.bottom.set_param(value),
      _ => Err("Invalid parameter index"),
    }
  }

  fn getParam(&mut self, index: i32) -> Var {
    match index {
      0 => self.top.get_param(),
      1 => self.left.get_param(),
      2 => self.center.get_param(),
      3 => self.right.get_param(),
      4 => self.bottom.get_param(),
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
      exposedType: EGUI_UI_SEQ_TYPE,
      name: self.ui_ctx_instance.get_name(),
      help: cstr!("The parent UI objects.").into(),
      isMutable: false,
      isProtected: true, // don't allow to be used in code/wires
      isTableEntry: false,
      global: false,
    };
    shared.push(ui_info);
    // update shared
    data.shared = (&shared).into();

    if !self.top.is_empty() {
      self.top.compose(&data)?;
    }

    if !self.left.is_empty() {
      self.left.compose(&data)?;
    }

    if !self.right.is_empty() {
      self.right.compose(&data)?;
    }

    if !self.bottom.is_empty() {
      self.bottom.compose(&data)?;
    }

    // center always last
    if !self.center.is_empty() {
      self.center.compose(&data)?;
    }

    Ok(data.inputType)
  }

  fn warmup(&mut self, ctx: &Context) -> Result<(), &str> {
    self.instance.warmup(ctx);
    self.ui_ctx_instance.warmup(ctx);

    if !self.top.is_empty() {
      self.top.warmup(ctx)?;
    }

    if !self.left.is_empty() {
      self.left.warmup(ctx)?;
    }

    if !self.right.is_empty() {
      self.right.warmup(ctx)?;
    }

    if !self.bottom.is_empty() {
      self.bottom.warmup(ctx)?;
    }

    // center always last
    if !self.center.is_empty() {
      self.center.warmup(ctx)?;
    }

    Ok(())
  }

  fn cleanup(&mut self) -> Result<(), &str> {
    if !self.top.is_empty() {
      self.top.cleanup();
    }

    if !self.left.is_empty() {
      self.left.cleanup();
    }

    if !self.right.is_empty() {
      self.right.cleanup();
    }

    if !self.bottom.is_empty() {
      self.bottom.cleanup();
    }

    // center always last
    if !self.center.is_empty() {
      self.center.cleanup();
    }

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

    if !self.top.is_empty() {
      egui::TopBottomPanel::top(EguiId::new(self, 0)).show(gui_ctx, |ui| {
        // pass the ui parent to the inner shards
        unsafe {
          let var = Var::new_object_from_ptr(ui as *const _, &EGUI_UI_TYPE);
          let mut seq = Seq::new();
          seq.push(var);
          self.ui_ctx_instance.set(seq.as_ref().into());
        }

        let mut output = Var::default();
        if self.top.activate(context, input, &mut output) == WireState::Error {
          failed = true;
          return;
        }
      });
      if failed {
        // TODO add a parameter where we can set to allow some panels to fail!
        return Err("Failed to activate top panel");
      }
    }

    if !self.left.is_empty() {
      egui::SidePanel::left(EguiId::new(self, 1)).show(gui_ctx, |ui| {
        // pass the ui parent to the inner shards
        unsafe {
          let var = Var::new_object_from_ptr(ui as *const _, &EGUI_UI_TYPE);
          let mut seq = Seq::new();
          seq.push(var);
          self.ui_ctx_instance.set(seq.as_ref().into());
        }

        let mut output = Var::default();
        if self.left.activate(context, input, &mut output) == WireState::Error {
          failed = true;
          return;
        }
      });
      if failed {
        return Err("Failed to activate left panel");
      }
    }

    if !self.right.is_empty() {
      egui::SidePanel::right(EguiId::new(self, 2)).show(gui_ctx, |ui| {
        // pass the ui parent to the inner shards
        unsafe {
          let var = Var::new_object_from_ptr(ui as *const _, &EGUI_UI_TYPE);
          let mut seq = Seq::new();
          seq.push(var);
          self.ui_ctx_instance.set(seq.as_ref().into());
        }

        let mut output = Var::default();
        if self.right.activate(context, input, &mut output) == WireState::Error {
          failed = true;
          return;
        }
      });
      if failed {
        return Err("Failed to activate right panel");
      }
    }

    if !self.bottom.is_empty() {
      egui::TopBottomPanel::bottom(EguiId::new(self, 3)).show(gui_ctx, |ui| {
        // pass the ui parent to the inner shards
        unsafe {
          let var = Var::new_object_from_ptr(ui as *const _, &EGUI_UI_TYPE);
          let mut seq = Seq::new();
          seq.push(var);
          self.ui_ctx_instance.set(seq.as_ref().into());
        }

        let mut output = Var::default();
        if self.bottom.activate(context, input, &mut output) == WireState::Error {
          failed = true;
          return;
        }
      });
      if failed {
        return Err("Failed to activate bottom panel");
      }
    }

    // center always last
    if !self.center.is_empty() {
      egui::CentralPanel::default().show(gui_ctx, |ui| {
        // pass the ui parent to the inner shards
        unsafe {
          let var = Var::new_object_from_ptr(ui as *const _, &EGUI_UI_TYPE);
          let mut seq = Seq::new();
          seq.push(var);
          self.ui_ctx_instance.set(seq.as_ref().into());
        }

        let mut output = Var::default();
        if self.center.activate(context, input, &mut output) == WireState::Error {
          failed = true;
          return;
        }
      });
      if failed {
        return Err("Failed to activate center panel");
      }
    }

    Ok(*input)
  }
}
