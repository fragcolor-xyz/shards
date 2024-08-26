/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

use super::AddFont;
use crate::util;
use crate::CONTEXTS_NAME;
use crate::EGUI_CTX_TYPE;
use shards::shard::LegacyShard;
use shards::types::Context;
use shards::types::ExposedInfo;
use shards::types::ExposedTypes;
use shards::types::OptionalString;
use shards::types::ParamVar;
use shards::types::SeqVar;
use shards::types::Types;
use shards::types::Var;
use shards::types::SEQ_OF_SEQ_OF_ANY_TYPES;

impl Default for AddFont {
  fn default() -> Self {
    let mut ctx = ParamVar::default();
    ctx.set_name(CONTEXTS_NAME);
    Self {
      instance: ctx,
      requiring: Vec::new(),
    }
  }
}

impl LegacyShard for AddFont {
  fn registerName() -> &'static str
  where
    Self: Sized,
  {
    cstr!("UI.AddFonts")
  }

  fn hash() -> u32
  where
    Self: Sized,
  {
    compile_time_crc32::crc32!("UI.AddFonts-rust-0x20200101")
  }

  fn name(&mut self) -> &str {
    "UI.AddFonts"
  }

  fn help(&mut self) -> OptionalString {
    OptionalString(shccstr!(
      "Adds custom fonts to the UI system, the input should a seq of a pair (name, bytes)."
    ))
  }

  fn inputTypes(&mut self) -> &Types {
    &SEQ_OF_SEQ_OF_ANY_TYPES
  }

  fn outputTypes(&mut self) -> &Types {
    &SEQ_OF_SEQ_OF_ANY_TYPES
  }

  fn requiredVariables(&mut self) -> Option<&ExposedTypes> {
    self.requiring.clear();

    // Add UI.Contexts to the list of required variables
    let exp_info = ExposedInfo {
      exposedType: EGUI_CTX_TYPE,
      name: self.instance.get_name(),
      help: shccstr!("The exposed UI context."),
      ..ExposedInfo::default()
    };
    self.requiring.push(exp_info);

    Some(&self.requiring)
  }

  fn warmup(&mut self, ctx: &Context) -> Result<(), &str> {
    self.instance.warmup(ctx);

    Ok(())
  }

  fn cleanup(&mut self, ctx: Option<&Context>) -> Result<(), &str> {
    self.instance.cleanup(ctx);

    Ok(())
  }

  fn activate(&mut self, _context: &Context, input: &Var) -> Result<Option<Var>, &str> {
    let egui_ctx = &util::get_current_context(&self.instance)?.egui_ctx;
    let pairs = SeqVar::wrap(*input);

    let mut fonts = egui::FontDefinitions::default();

    for pair in pairs.iter() {
      let pair = SeqVar::wrap(pair);
      if pair.len() != 2 {
        return Err("Font pair must be a sequence of two elements");
      }
      let name: &str = pair[0].as_ref().try_into()?;
      let bytes: &[u8] = pair[1].as_ref().try_into()?;
      fonts
        .font_data
        .insert(name.to_owned(), egui::FontData::from_static(bytes));
    }

    egui_ctx.set_fonts(fonts);

    Ok(None)
  }
}
