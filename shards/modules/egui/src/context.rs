/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

use crate::egui_host::EguiHost;
use crate::util;
use crate::EguiContext;
use crate::CONTEXTS_NAME;
use crate::GFX_CONTEXT_TYPE;
use crate::GFX_QUEUE_VAR_TYPES;
use crate::HELP_OUTPUT_EQUAL_INPUT;
use crate::INPUT_CONTEXT_TYPE;
use crate::PARENTS_UI_NAME;
use crate::bindings;
use shards::shard::Shard;
use shards::shardsc;
use shards::types::Context;
use shards::types::ExposedInfo;
use shards::types::ExposedTypes;
use shards::types::InstanceData;
use shards::types::OptionalString;
use shards::types::ParamVar;
use shards::types::Parameters;
use shards::types::ShardsVar;
use shards::types::Type;
use shards::types::Var;
use shards::types::ANY_TYPES;
use shards::types::SHARDS_OR_NONE_TYPES;
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
      exposing: Vec::new(),
      graphics_context: unsafe {
        let mut var = ParamVar::default();
        let name = bindings::gfx_getGraphicsContextVarName() as shardsc::SHString;
        var.set_name(CStr::from_ptr(name).to_str().unwrap());
        var
      },
      input_context: unsafe {
        let mut var = ParamVar::default();
        let name = bindings::gfx_getInputContextVarName() as shardsc::SHString;
        var.set_name(CStr::from_ptr(name).to_str().unwrap());
        var
      },
      renderer: bindings::Renderer::new(),
      input_translator: bindings::InputTranslator::new(),
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
      let result = self.contents.compose(&data)?;
      return Ok(result.outputType);
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
      &*(crate::bindings::gfx_getEguiWindowInputs(
        self.input_translator.as_mut_ptr() as *mut bindings::gfx_EguiInputTranslator,
        self.graphics_context.get() as *const _ as *const bindings::SHVar,
        self.input_context.get() as *const _ as *const bindings::SHVar,
        1.0,
      ) as *const bindings::egui_Input)
    };

    self
      .host
      .activate(&egui_input, &(&self.contents).into(), context, input)?;
    let egui_output = self.host.get_egui_output();

    let queue_var = self.queue.get();
    unsafe {
      // Apply outputs to input related functionality (clipboard, cursor, etc.)
      bindings::gfx_EguiInputTranslator_applyOutput(
        self.input_translator.as_mut_ptr(),
        egui_output,
      );

      let queue = bindings::gfx_getDrawQueueFromVar(queue_var as *const _ as *const bindings::SHVar);
      self
        .renderer
        .render_with_native_output(egui_output, queue as *const bindings::gfx_DrawQueuePtr);
    }

    Ok(*input)
  }
}