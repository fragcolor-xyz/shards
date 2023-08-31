/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

use crate::bindings;
use crate::egui_host::EguiHost;
use crate::util;

use crate::GFX_CONTEXT_TYPE;
use crate::GFX_QUEUE_VAR_TYPES;
use crate::HELP_OUTPUT_EQUAL_INPUT;
use crate::INPUT_CONTEXT_TYPE;

use shards::core::register_shard;
use shards::shard::LegacyShard;
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

#[derive(shards::shard)]
#[shard_info("UI", "Initializes a UI context")]
struct EguiContext {
  #[shard_param("Queue", "The draw queue.", GFX_QUEUE_VAR_TYPES)]
  queue: ParamVar,
  #[shard_param("Contents", "The UI contents.", SHARDS_OR_NONE_TYPES)]
  contents: ShardsVar,
  host: EguiHost,
  #[shard_required]
  requiring: ExposedTypes,
  exposing: ExposedTypes,
  #[shard_warmup] 
  input_context: ParamVar,
  has_graphics_context: bool,
  graphics_context: ParamVar,
  renderer: bindings::Renderer,
  input_translator: bindings::InputTranslator,
}

impl Default for EguiContext {
  fn default() -> Self {
    Self {
      host: EguiHost::default(),
      requiring: Vec::new(),
      queue: ParamVar::default(),
      contents: ShardsVar::default(),
      exposing: Vec::new(),
      has_graphics_context: false,
      graphics_context: unsafe {
        ParamVar::new_named(
          CStr::from_ptr(bindings::gfx_getGraphicsContextVarName())
            .to_str()
            .unwrap(),
        )
      },
      input_context: unsafe {
        ParamVar::new_named(
          CStr::from_ptr(bindings::gfx_getInputContextVarName())
            .to_str()
            .unwrap(),
        )
      },
      renderer: bindings::Renderer::new(),
      input_translator: bindings::InputTranslator::new(),
    }
  }
}

#[shards::shard_impl]
impl Shard for EguiContext {
  fn input_types(&mut self) -> &std::vec::Vec<Type> {
    &ANY_TYPES
  }

  fn input_help(&mut self) -> OptionalString {
    OptionalString(shccstr!(
      "The value that will be passed to the Contents shards of the UI."
    ))
  }

  fn output_types(&mut self) -> &std::vec::Vec<Type> {
    &ANY_TYPES
  }

  fn output_help(&mut self) -> OptionalString {
    *HELP_OUTPUT_EQUAL_INPUT
  }

  fn exposed_variables(&mut self) -> Option<&ExposedTypes> {
    Some(&self.exposing)
  }

  fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
    self.compose_helper(data)?;

    // Add Graphics context to the list of required variables (optional)
    // If this is not provided, the UI will be rendered based on the window surface area
    self.has_graphics_context = (&data.shared)
      .iter()
      .find(|x| unsafe {
        CStr::from_ptr(x.name) == CStr::from_ptr(self.graphics_context.get_name())
      })
      .is_some();
    if self.has_graphics_context {
      let exp_info = ExposedInfo {
        exposedType: *GFX_CONTEXT_TYPE,
        name: self.graphics_context.get_name(),
        help: cstr!("The graphics context.").into(),
        ..ExposedInfo::default()
      };
      self.requiring.push(exp_info);
    }

    // Add Input context to the list of required variables
    let exp_info = ExposedInfo {
      exposedType: *INPUT_CONTEXT_TYPE,
      name: self.input_context.get_name(),
      help: cstr!("The input context.").into(),
      ..ExposedInfo::default()
    };
    self.requiring.push(exp_info);

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

    let output_type = if !self.contents.is_empty() {
      let result = self.contents.compose(&data)?;
      result.outputType
    } else {
      data.inputType
    };

    self.exposing.clear();
    util::expose_contents_variables(&mut self.exposing, &self.contents);

    // Always passthrough the input
    Ok(output_type)
  }

  fn warmup(&mut self, ctx: &Context) -> Result<(), &str> {
    self.host.warmup(ctx)?;
    self.warmup_helper(ctx)?;
    if self.has_graphics_context {
      self.graphics_context.warmup(ctx);
    }

    Ok(())
  }

  fn cleanup(&mut self) -> Result<(), &str> {
    self.cleanup_helper()?;
    self.graphics_context.cleanup();
    self.host.cleanup()?;
    Ok(())
  }

  fn activate(&mut self, context: &Context, input: &Var) -> Result<Var, &str> {
    if self.contents.is_empty() {
      return Ok(*input);
    }

    let egui_input = unsafe {
      let gfx_context = if self.has_graphics_context {
        self.graphics_context.get()
      } else {
        std::ptr::null()
      };

      &*(bindings::gfx_getEguiWindowInputs(
        self.input_translator.as_mut_ptr() as *mut bindings::gfx_EguiInputTranslator,
        gfx_context as *const _ as *const bindings::SHVar,
        self.input_context.get() as *const _ as *const bindings::SHVar,
        1.0,
      ) as *const bindings::egui_Input)
    };

    if egui_input.pixelsPerPoint > 0.0 {
      self
        .host
        .activate(&egui_input, &(&self.contents).into(), context, input)?;
      let egui_output = self.host.get_egui_output();

      let queue_var = self.queue.get();
      unsafe {
        bindings::gfx_applyEguiOutputs(
          self.input_translator.as_mut_ptr() as *mut bindings::gfx_EguiInputTranslator,
          egui_output as *const _,
          self.input_context.get() as *const _ as *const bindings::SHVar,
        );

        let queue =
          bindings::gfx_getDrawQueueFromVar(queue_var as *const _ as *const bindings::SHVar);
        self
          .renderer
          .render_with_native_output(egui_output, queue as *const bindings::gfx_DrawQueuePtr);
      }
    }

    Ok(*input)
  }
}

pub fn register_shards() {
  register_shard::<EguiContext>();
}
