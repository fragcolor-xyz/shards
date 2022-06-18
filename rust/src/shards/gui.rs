/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

use crate::core::registerShard;
use crate::shard::Shard;
use crate::types::Context;
use crate::types::ExposedInfo;
use crate::types::ExposedTypes;
use crate::types::InstanceData;
use crate::types::ParamVar;
use crate::types::Parameters;
use crate::types::ShardsVar;
use crate::types::Type;
use crate::types::Var;
use crate::types::WireState;
use crate::types::FRAG_CC;
use crate::types::NONE_TYPES;
use crate::types::SHARDS_OR_NONE_TYPES;
use crate::types::{RawString, Types};
use egui::containers::panel::{CentralPanel, SidePanel, TopBottomPanel};
use egui::Context as EguiNativeContext;
use egui::Ui;
use std::ffi::c_void;
use std::ffi::CString;
use std::rc::Rc;

static EGUI_UI_TYPE: Type = Type::object(FRAG_CC, 1701279061); // 'eguU'
static EGUI_UI_SLICE: &'static [Type] = &[EGUI_UI_TYPE];
static EGUI_UI_VAR: Type = Type::context_variable(EGUI_UI_SLICE);

static EGUI_CTX_TYPE: Type = Type::object(FRAG_CC, 1701279043); // 'eguC'
static EGUI_CTX_SLICE: &'static [Type] = &[EGUI_CTX_TYPE];
static EGUI_CTX_VAR: Type = Type::context_variable(EGUI_CTX_SLICE);
static EGUI_CTX_VAR_TYPES: &'static [Type] = &[EGUI_CTX_VAR];

lazy_static! {
  static ref EGUI_CTX_VEC: Types = vec![EGUI_CTX_TYPE];
  static ref PANELS_PARAMETERS: Parameters = vec![
    (
      cstr!("Context"),
      cstr!("The UI context to render panels onto."),
      EGUI_CTX_VAR_TYPES,
    )
      .into(),
    (
      cstr!("Top"),
      cstr!("A panel that covers the entire top of a UI surface."),
      &SHARDS_OR_NONE_TYPES[..],
    )
      .into(),
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
    )
      .into(),
  ];
}

#[derive(Hash)]
struct EguiId {
  p: usize,
  idx: u8,
}

impl EguiId {
  fn new(shard: &dyn Shard, idx: u8) -> EguiId {
    EguiId {
      p: shard as *const dyn Shard as *mut c_void as usize,
      idx,
    }
  }
}

// The root of a GUI tree.
// This could be flat on the screen, or it could be attached to 3D geometry.
struct EguiContext {
  context: Rc<Option<EguiNativeContext>>,
}

impl Default for EguiContext {
  fn default() -> Self {
    Self {
      context: Rc::new(None),
    }
  }
}

impl Shard for EguiContext {
  fn registerName() -> &'static str {
    cstr!("GUI")
  }

  fn hash() -> u32 {
    compile_time_crc32::crc32!("GUI-rust-0x20200101")
  }

  fn name(&mut self) -> &str {
    "GUI"
  }

  fn inputTypes(&mut self) -> &std::vec::Vec<Type> {
    &NONE_TYPES
  }

  fn outputTypes(&mut self) -> &std::vec::Vec<Type> {
    &EGUI_CTX_VEC
  }

  fn warmup(&mut self, _ctx: &Context) -> Result<(), &str> {
    self.context = Rc::new(Some(EguiNativeContext::default()));
    Ok(())
  }

  fn activate(&mut self, _: &Context, _input: &Var) -> Result<Var, &str> {
    let result = Var::new_object(&self.context, &EGUI_CTX_TYPE);
    Ok(result)
  }
}

struct Panels {
  context: Option<Rc<Option<EguiNativeContext>>>,
  instance: ParamVar, // Context parameter, this will go will with trait system (users able to plug into existing UIs and interop with them)
  requiring: ExposedTypes,
  top: ShardsVar,
  left: ShardsVar,
  center: ShardsVar,
  right: ShardsVar,
  bottom: ShardsVar,
  ui_ctx_instance: ParamVar,
  ui_ctx_rc: Rc<Option<Ui>>,
}

impl Default for Panels {
  fn default() -> Self {
    let mut ui_ctx = ParamVar::new(().into());
    ui_ctx.set_name("GUI.UI.Parent");
    Self {
      context: None,
      instance: ParamVar::new(().into()),
      requiring: Vec::new(),
      top: ShardsVar::default(),
      left: ShardsVar::default(),
      center: ShardsVar::default(),
      right: ShardsVar::default(),
      bottom: ShardsVar::default(),
      ui_ctx_instance: ui_ctx,
      ui_ctx_rc: Rc::new(None),
    }
  }
}

impl Shard for Panels {
  fn registerName() -> &'static str {
    cstr!("GUI.Panels")
  }

  fn hash() -> u32 {
    compile_time_crc32::crc32!("GUI.Panels-rust-0x20200101")
  }

  fn name(&mut self) -> &str {
    "GUI.Panels"
  }

  fn inputTypes(&mut self) -> &std::vec::Vec<Type> {
    &NONE_TYPES
  }

  fn outputTypes(&mut self) -> &std::vec::Vec<Type> {
    &NONE_TYPES
  }

  fn parameters(&mut self) -> Option<&Parameters> {
    Some(&PANELS_PARAMETERS)
  }

  fn setParam(&mut self, index: i32, value: &Var) -> Result<(), &str> {
    match index {
      0 => Ok(self.instance.set_param(value)),
      1 => self.top.set_param(value),
      2 => self.left.set_param(value),
      3 => self.center.set_param(value),
      4 => self.right.set_param(value),
      5 => self.bottom.set_param(value),
      _ => Err("Invalid parameter index"),
    }
  }

  fn getParam(&mut self, index: i32) -> Var {
    match index {
      0 => self.instance.get_param(),
      1 => self.top.get_param(),
      2 => self.left.get_param(),
      3 => self.center.get_param(),
      4 => self.right.get_param(),
      5 => self.bottom.get_param(),
      _ => Var::default(),
    }
  }

  fn requiredVariables(&mut self) -> Option<&ExposedTypes> {
    if !self.instance.is_variable() {
      return None;
    }

    self.requiring.clear();

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
      name: shstr!("GUI.UI.Parent"),
      help: cstr!("The parent UI object.").into(),
      isMutable: false,
      isProtected: true, // don't allow to be used in code/wires
      isTableEntry: false,
      global: false,
      scope: core::ptr::null_mut(),
    };
    shared.push(ui_info);
    // Copy on top of copy
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
    if !self.instance.is_variable() {
      return Err("No UI context variable");
    }

    self
      .ui_ctx_instance
      .set(Var::new_object(&self.ui_ctx_rc, &EGUI_UI_TYPE));

    self.context = Some(Var::from_object_as_clone(
      self.instance.get(),
      &EGUI_CTX_TYPE,
    )?);

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

    self.context = None; // release RC etc

    Ok(())
  }

  fn activate(&mut self, context: &Context, input: &Var) -> Result<Var, &str> {
    let gui_ctx = Var::get_mut_from_clone(&self.context)?;

    let mut failed = false;

    if !self.top.is_empty() {
      TopBottomPanel::top(EguiId::new(self, 0)).show(gui_ctx, |ui| {
        // pass the ui parent to the inner shards
        unsafe {
          let var = Var::new_object_from_ptr(ui as *const _, &EGUI_UI_TYPE);
          self.ui_ctx_instance.set(var);
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
      SidePanel::left(EguiId::new(self, 1)).show(gui_ctx, |ui| {
        // pass the ui parent to the inner shards
        unsafe {
          let var = Var::new_object_from_ptr(ui as *const _, &EGUI_UI_TYPE);
          self.ui_ctx_instance.set(var);
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
      SidePanel::right(EguiId::new(self, 2)).show(gui_ctx, |ui| {
        // pass the ui parent to the inner shards
        unsafe {
          let var = Var::new_object_from_ptr(ui as *const _, &EGUI_UI_TYPE);
          self.ui_ctx_instance.set(var);
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
      TopBottomPanel::bottom(EguiId::new(self, 3)).show(gui_ctx, |ui| {
        // pass the ui parent to the inner shards
        unsafe {
          let var = Var::new_object_from_ptr(ui as *const _, &EGUI_UI_TYPE);
          self.ui_ctx_instance.set(var);
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
      CentralPanel::default().show(gui_ctx, |ui| {
        // pass the ui parent to the inner shards
        unsafe {
          let var = Var::new_object_from_ptr(ui as *const _, &EGUI_UI_TYPE);
          self.ui_ctx_instance.set(var);
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

struct Label {
  parent: ParamVar,
  text: Option<CString>,
}

impl Default for Label {
  fn default() -> Self {
    let mut ui_ctx = ParamVar::new(().into());
    ui_ctx.set_name("GUI.UI.Parent");
    Label {
      parent: ui_ctx,
      text: None,
    }
  }
}

impl Shard for Label {
  fn registerName() -> &'static str
  where
    Self: Sized,
  {
    todo!()
  }

  fn hash() -> u32
  where
    Self: Sized,
  {
    todo!()
  }

  fn name(&mut self) -> &str {
    todo!()
  }

  fn inputTypes(&mut self) -> &Types {
    todo!()
  }

  fn outputTypes(&mut self) -> &Types {
    todo!()
  }

  fn requiredVariables(&mut self) -> Option<&ExposedTypes> {
    // TODO Add GUI.UI.Parent to the list of required variables
    todo!()
  }

  fn warmup(&mut self, context: &Context) -> Result<(), &str> {
    self.parent.warmup(context);
    Ok(())
  }

  fn cleanup(&mut self) -> Result<(), &str> {
    self.parent.cleanup();
    Ok(())
  }

  fn activate(&mut self, context: &Context, input: &Var) -> Result<Var, &str> {
    let ui = {
      let ui_ptr: &mut Ui = Var::from_object_ptr_mut_ref(self.parent.get(), &EGUI_UI_TYPE)?;
      &*ui_ptr
    };

    // TODO use ui regularly

    todo!()
  }
}

pub fn registerShards() {
  registerShard::<EguiContext>();
  registerShard::<Panels>();
}
