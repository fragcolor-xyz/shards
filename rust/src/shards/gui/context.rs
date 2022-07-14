/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

use super::EguiContext;
use super::CONTEXT_NAME;
use super::EGUI_CTX_TYPE;
use super::GFX_GLOBALS_TYPE;
use super::GFX_QUEUE_VAR_TYPES;
use super::PARENTS_UI_NAME;
use crate::shard::Shard;
use crate::shardsc;
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
use egui::RawInput;
use std::ffi::CStr;

lazy_static! {
  static ref CONTEXT_PARAMETERS: Parameters = vec![
    (
      cstr!("Queue"),
      cstr!("The draw queue"),
      &GFX_QUEUE_VAR_TYPES[..]
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

impl Default for EguiContext {
  fn default() -> Self {
    let mut ctx = ParamVar::default();
    ctx.set_name(CONTEXT_NAME);

    let mut mw_globals = ParamVar::default();
    unsafe {
      let gfx_globals_var_name = shardsc::gfx_getMainWindowGlobalsVarName() as shardsc::SHString;
      mw_globals.set_name(CStr::from_ptr(gfx_globals_var_name).to_str().unwrap());
    }

    let mut parents = ParamVar::default();
    parents.set_name(PARENTS_UI_NAME);

    Self {
      context: None,
      instance: ctx,
      requiring: Vec::new(),
      queue: ParamVar::default(),
      contents: ShardsVar::default(),
      main_window_globals: mw_globals,
      parents,
      renderer: egui_gfx::Renderer::new(),
      input_translator: egui_gfx::InputTranslator::new(),
    }
  }
}

impl Shard for EguiContext {
  fn registerName() -> &'static str {
    cstr!("UI")
  }

  fn hash() -> u32 {
    compile_time_crc32::crc32!("UI-rust-0x20200101")
  }

  fn name(&mut self) -> &str {
    "UI"
  }

  fn help(&mut self) -> OptionalString {
    OptionalString(shccstr!("Initializes a UI context."))
  }

  fn inputTypes(&mut self) -> &std::vec::Vec<Type> {
    &ANY_TYPES
  }

  fn outputTypes(&mut self) -> &std::vec::Vec<Type> {
    &ANY_TYPES
  }

  fn parameters(&mut self) -> Option<&Parameters> {
    Some(&CONTEXT_PARAMETERS)
  }

  fn setParam(&mut self, index: i32, value: &Var) -> Result<(), &str> {
    match index {
      0 => Ok(self.queue.set_param(value)),
      1 => self.contents.set_param(value),
      _ => Err("Invalid parameter index"),
    }
  }

  fn getParam(&mut self, index: i32) -> Var {
    match index {
      0 => self.queue.get_param(),
      1 => self.contents.get_param(),
      _ => Var::default(),
    }
  }

  fn requiredVariables(&mut self) -> Option<&ExposedTypes> {
    self.requiring.clear();

    // Add GFX.MainWindow to the list of required variables
    let exp_info = ExposedInfo {
      exposedType: *GFX_GLOBALS_TYPE,
      name: self.main_window_globals.get_name(),
      help: cstr!("The exposed main window.").into(),
      ..ExposedInfo::default()
    };
    self.requiring.push(exp_info);

    Some(&self.requiring)
  }

  fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
    // we need to inject the UI context to the inner shards
    let mut data = *data;
    // clone shared into a new vector we can append things to
    let mut shared: ExposedTypes = data.shared.into();
    // append to shared ui vars
    let ctx_info = ExposedInfo {
      exposedType: EGUI_CTX_TYPE,
      name: self.instance.get_name(),
      help: cstr!("The UI context.").into(),
      isMutable: false,
      isProtected: true, // don't allow to be used in code/wires
      isTableEntry: false,
      global: false,
    };
    shared.push(ctx_info);
    // update shared
    data.shared = (&shared).into();

    if !self.contents.is_empty() {
      let outputType = self.contents.compose(&data)?;
      return Ok(outputType);
    }

    Ok(data.inputType)
  }

  fn warmup(&mut self, ctx: &Context) -> Result<(), &str> {
    self.context = Some(EguiNativeContext::default());
    self.instance.warmup(ctx);
    self.queue.warmup(ctx);
    self.contents.warmup(ctx)?;
    self.main_window_globals.warmup(ctx);
    self.parents.warmup(ctx);
    // Initialize the parents stack in the root UI.
    // Every other UI elements will reference it and push or pop UIs to it.
    self.parents.set(Seq::new().as_ref().into());
    Ok(())
  }

  fn cleanup(&mut self) -> Result<(), &str> {
    self.parents.cleanup();
    self.main_window_globals.cleanup();
    self.contents.cleanup();
    self.queue.cleanup();
    self.instance.cleanup();
    Ok(())
  }

  fn activate(&mut self, context: &Context, input: &Var) -> Result<Var, &str> {
    let gui_ctx = if let Some(gui_ctx) = &self.context {
      gui_ctx
    } else {
      return Err("No UI context");
    };

    if self.contents.is_empty() {
      return Ok(*input);
    }

    let raw_input = unsafe {
      let inputs = shardsc::gfx_getEguiWindowInputs(
        self.input_translator.as_mut_ptr() as *mut shardsc::gfx_EguiInputTranslator,
        self.main_window_globals.get(),
      ) as *const egui_gfx::egui_Input;
      egui_gfx::translate_raw_input(&*inputs)
    };

    match raw_input {
      Err(error) => {
        shlog_debug!("Input translation error: {:?}", error);
        Err("Input translation error")
      }
      Ok(raw_input) => {
        let draw_scale = raw_input.pixels_per_point.unwrap_or(1.0);

        let mut failed = false;
        let mut output = Var::default();
        let egui_output = gui_ctx.run(raw_input, |ctx| {
          unsafe {
            let var = Var::new_object_from_ptr(ctx as *const _, &EGUI_CTX_TYPE);
            self.instance.set(var);
          }

          if self.contents.activate(context, input, &mut output) == WireState::Error {
            failed = true;
            return;
          }
        });
        if failed {
          return Err("Failed to activate UI contents");
        }

        let queue_var = self.queue.get();
        unsafe {
          let queue = shardsc::gfx_getDrawQueueFromVar(queue_var);
          self.renderer.render(
            &gui_ctx,
            egui_output,
            queue as *const egui_gfx::gfx_DrawQueuePtr,
            draw_scale,
          )?;
        }

        Ok(output)
      }
    }
  }
}
