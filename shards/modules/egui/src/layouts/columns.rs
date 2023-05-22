/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

use super::Columns;
use shards::shard::Shard;
use crate::util;
use crate::HELP_OUTPUT_EQUAL_INPUT;
use crate::PARENTS_UI_NAME;
use shards::types::Context;
use shards::types::ExposedTypes;
use shards::types::InstanceData;
use shards::types::OptionalString;
use shards::types::ParamVar;
use shards::types::Parameters;
use shards::types::Seq;
use shards::types::ShardsVar;
use shards::types::Type;
use shards::types::Types;
use shards::types::Var;
use shards::types::ANY_TYPES;
use shards::types::SEQ_OF_SHARDS_TYPES;

lazy_static! {
  static ref COLUMNS_PARAMETERS: Parameters = vec![(
    cstr!("Contents"),
    shccstr!("A sequence of UI contents."),
    &SEQ_OF_SHARDS_TYPES[..],
  )
    .into(),];
}

impl Default for Columns {
  fn default() -> Self {
    let mut parents = ParamVar::default();
    parents.set_name(PARENTS_UI_NAME);
    Self {
      parents,
      requiring: Vec::new(),
      contents: ParamVar::new(Seq::new().as_ref().into()),
      shards: Vec::new(),
      exposing: Vec::new(),
    }
  }
}

impl Shard for Columns {
  fn registerName() -> &'static str
  where
    Self: Sized,
  {
    cstr!("UI.Columns")
  }

  fn hash() -> u32
  where
    Self: Sized,
  {
    compile_time_crc32::crc32!("UI.Columns-rust-0x20200101")
  }

  fn name(&mut self) -> &str {
    "UI.Columns"
  }

  fn help(&mut self) -> OptionalString {
    OptionalString(shccstr!("Splits the contents into several columns."))
  }

  fn inputTypes(&mut self) -> &Types {
    &ANY_TYPES
  }

  fn inputHelp(&mut self) -> OptionalString {
    OptionalString(shccstr!(
      "The value that will be passed to the Contents shards (each column)."
    ))
  }

  fn outputTypes(&mut self) -> &Types {
    &ANY_TYPES
  }

  fn outputHelp(&mut self) -> OptionalString {
    *HELP_OUTPUT_EQUAL_INPUT
  }

  fn parameters(&mut self) -> Option<&Parameters> {
    Some(&COLUMNS_PARAMETERS)
  }

  fn setParam(&mut self, index: i32, value: &Var) -> Result<(), &str> {
    match index {
      0 => {
        let seq = Seq::try_from(value)?;
        for shard in seq.iter() {
          let mut s = ShardsVar::default();
          s.set_param(&shard)?;
          self.shards.push(s);
        }
        Ok(self.contents.set_param(value))
      }
      _ => Err("Invalid parameter index"),
    }
  }

  fn getParam(&mut self, index: i32) -> Var {
    match index {
      0 => self.contents.get_param(),
      _ => Var::default(),
    }
  }

  fn requiredVariables(&mut self) -> Option<&ExposedTypes> {
    self.requiring.clear();

    // Add UI.Parents to the list of required variables
    util::require_parents(&mut self.requiring, &self.parents);

    Some(&self.requiring)
  }

  fn exposedVariables(&mut self) -> Option<&ExposedTypes> {
    self.exposing.clear();

    let mut exposed = false;
    for s in &self.shards {
      exposed |= util::expose_contents_variables(&mut self.exposing, s);
    }
    if exposed {
      Some(&self.exposing)
    } else {
      None
    }
  }

  fn hasCompose() -> bool {
    true
  }

  fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
    for s in &mut self.shards {
      s.compose(data)?;
    }

    // Always passthrough the input
    Ok(data.inputType)
  }

  fn warmup(&mut self, ctx: &Context) -> Result<(), &str> {
    self.parents.warmup(ctx);
    self.contents.warmup(ctx);
    for s in &mut self.shards {
      s.warmup(ctx)?;
    }

    Ok(())
  }

  fn cleanup(&mut self) -> Result<(), &str> {
    for s in &mut self.shards {
      s.cleanup();
    }
    self.contents.cleanup();
    self.parents.cleanup();

    Ok(())
  }

  fn activate(&mut self, context: &Context, input: &Var) -> Result<Var, &str> {
    let len = self.shards.len();

    if len == 0 {
      return Ok(*input);
    }

    if let Some(ui) = util::get_current_parent(self.parents.get())? {
      ui.columns(len, |columns| {
        for (i, item) in columns.iter_mut().enumerate() {
          util::activate_ui_contents(context, input, item, &mut self.parents, &mut self.shards[i])?;
        }
        Ok::<(), &str>(())
      })?;

      // Always passthrough the input
      Ok(*input)
    } else {
      Err("No UI parent")
    }
  }
}
