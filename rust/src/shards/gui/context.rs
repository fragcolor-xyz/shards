/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

use egui_gfx::gfx_EguiRenderer_renderNoTransform;

use super::egui_host::EguiHost;
use super::util;
use super::EguiContext;
use super::CONTEXTS_NAME;
use super::EGUI_CTX_TYPE;
use super::EGUI_UI_SEQ_TYPE;
use super::GFX_CONTEXT_TYPE;
use super::GFX_QUEUE_VAR_TYPES;
use super::HELP_OUTPUT_EQUAL_INPUT;
use super::INPUT_CONTEXT_TYPE;
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
use std::ffi::CStr;

lazy_static! {
  static ref CONTEXT_PARAMETERS: Parameters = vec![
    (
      cstr!("Queue"),
      shccstr!("The draw queue."),
      &GFX_QUEUE_VAR_TYPES[..]
    )
      .into(),
    (
      cstr!("Contents"),
      shccstr!("The UI contents."),
      &SHARDS_OR_NONE_TYPES[..],
    )
      .into(),
  ];
}

impl Default for EguiContext {
  fn default() -> Self {
    let mut ctx = ParamVar::default();
    ctx.set_name(CONTEXTS_NAME);

    let mut parents = ParamVar::default();
    parents.set_name(PARENTS_UI_NAME);

    Self {
      host: EguiHost::default(),
      requiring: Vec::new(),
      queue: ParamVar::default(),
      contents: ShardsVar::default(),
      graphics_context: unsafe {
        let mut var = ParamVar::default();
        let name = shardsc::gfx_getGraphicsContextVarName() as shardsc::SHString;
        var.set_name(CStr::from_ptr(name).to_str().unwrap());
        var
      },
      input_context: unsafe {
        let mut var = ParamVar::default();
        let name = shardsc::gfx_getInputContextVarName() as shardsc::SHString;
        var.set_name(CStr::from_ptr(name).to_str().unwrap());
        var
      },
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

  fn inputHelp(&mut self) -> OptionalString {
    OptionalString(shccstr!(
      "The value that will be passed to the Contents shards of the UI."
    ))
  }

  fn outputTypes(&mut self) -> &std::vec::Vec<Type> {
    &ANY_TYPES
  }

  fn outputHelp(&mut self) -> OptionalString {
    *HELP_OUTPUT_EQUAL_INPUT
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

    // Add Graphics context to the list of required variables
    let exp_info = ExposedInfo {
      exposedType: *GFX_CONTEXT_TYPE,
      name: self.graphics_context.get_name(),
      help: cstr!("The graphics context.").into(),
      ..ExposedInfo::default()
    };
    self.requiring.push(exp_info);

    // Add Input context to the list of required variables
    let exp_info = ExposedInfo {
      exposedType: *INPUT_CONTEXT_TYPE,
      name: self.input_context.get_name(),
      help: cstr!("The input context.").into(),
      ..ExposedInfo::default()
    };
    self.requiring.push(exp_info);

    Some(&self.requiring)
  }

  fn hasCompose() -> bool {
    true
  }

  fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
    // we need to inject the UI context to the inner shards
    let mut data = *data;
    // clone shared into a new vector we can append things to
    let mut shared: ExposedTypes = data.shared.into();

    // expose UI context
    for exposed in self.host.get_exposed_info() {
      shared.push(*exposed);
    }

    // update shared
    data.shared = (&shared).into();

    if !self.contents.is_empty() {
      let outputType = self.contents.compose(&data)?;
      return Ok(outputType);
    }

    // Always passthrough the input
    Ok(data.inputType)
  }

  fn warmup(&mut self, ctx: &Context) -> Result<(), &str> {
    self.host.warmup(ctx)?;
    self.queue.warmup(ctx);
    self.contents.warmup(ctx)?;
    self.graphics_context.warmup(ctx);
    self.input_context.warmup(ctx);

    Ok(())
  }

  fn cleanup(&mut self) -> Result<(), &str> {
    self.graphics_context.cleanup();
    self.input_context.cleanup();
    self.contents.cleanup();
    self.queue.cleanup();
    self.host.cleanup()?;
    Ok(())
  }

  fn activate(&mut self, context: &Context, input: &Var) -> Result<Var, &str> {
    if self.contents.is_empty() {
      return Ok(*input);
    }

    let egui_input = unsafe {
      &*(shardsc::gfx_getEguiWindowInputs(
        self.input_translator.as_mut_ptr() as *mut shardsc::gfx_EguiInputTranslator,
        self.graphics_context.get(),
        self.input_context.get(),
        1.0,
      ) as *const egui_gfx::egui_Input)
    };

    self
      .host
      .activate_shards(&egui_input, &(&self.contents).into(), context, input)?;
    let egui_output = self.host.get_egui_output();

    let queue_var = self.queue.get();
    unsafe {
      let queue = shardsc::gfx_getDrawQueueFromVar(queue_var);
      self
        .renderer
        .render_with_native_output(egui_output, queue as *const egui_gfx::gfx_DrawQueuePtr);
    }

    Ok(*input)
  }
}
