/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

use super::EguiContext;
use super::CONTEXT_NAME;
use super::EGUI_CTX_TYPE;
use crate::shard::Shard;
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
use crate::types::Var;
use crate::types::WireState;
use crate::types::ANY_TYPES;
use crate::types::SHARDS_OR_NONE_TYPES;
use egui::Context as EguiNativeContext;
use egui::RawInput;

lazy_static! {
  static ref CONTEXT_PARAMETERS: Parameters = vec![(
    cstr!("Contents"),
    cstr!("The UI contents."),
    &SHARDS_OR_NONE_TYPES[..],
  )
    .into(),];
}

impl Default for EguiContext {
  fn default() -> Self {
    let mut ctx = ParamVar::new(().into());
    ctx.set_name(CONTEXT_NAME);
    Self {
      context: None,
      instance: ctx,
      contents: ShardsVar::default(),
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
      0 => self.contents.set_param(value),
      _ => Err("Invalid parameter index"),
    }
  }

  fn getParam(&mut self, index: i32) -> Var {
    match index {
      0 => self.contents.get_param(),
      _ => Var::default(),
    }
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
    self.contents.warmup(ctx)?;
    Ok(())
  }

  fn cleanup(&mut self) -> Result<(), &str> {
    self.contents.cleanup();
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

    let raw_input = RawInput::default(); // FIXME: where do raw input come from?

    let mut failed = false;
    let mut output = Var::default();
    let fullOutput = gui_ctx.run(raw_input, |ctx| {
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

    Ok(output)
  }
}
