/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

use super::MarkdownViewer;
use crate::util;
use crate::EguiId;
use crate::HELP_OUTPUT_EQUAL_INPUT;
use crate::PARENTS_UI_NAME;
use shards::shard::LegacyShard;
use shards::types::Context;
use shards::types::ExposedTypes;
use shards::types::OptionalString;
use shards::types::ParamVar;
use shards::types::Types;
use shards::types::Var;
use shards::types::STRING_TYPES;

impl Default for MarkdownViewer {
  fn default() -> Self {
    let mut parents = ParamVar::default();
    parents.set_name(PARENTS_UI_NAME);
    let mut cache = egui_commonmark::CommonMarkCache::default();
    cache.add_syntax_from_str(
      include_str!("code_editor/sublime-syntax.yml"),
      Some("shards"),
    );
    Self {
      parents,
      requiring: Vec::new(),
      cache,
    }
  }
}

impl LegacyShard for MarkdownViewer {
  fn registerName() -> &'static str
  where
    Self: Sized,
  {
    cstr!("UI.MarkdownViewer")
  }

  fn hash() -> u32
  where
    Self: Sized,
  {
    compile_time_crc32::crc32!("UI.MarkdownViewer-rust-0x20200101")
  }

  fn name(&mut self) -> &str {
    "UI.MarkdownViewer"
  }

  fn help(&mut self) -> OptionalString {
    OptionalString(shccstr!("Renders a markdown text."))
  }

  fn inputTypes(&mut self) -> &Types {
    &STRING_TYPES
  }

  fn inputHelp(&mut self) -> OptionalString {
    OptionalString(shccstr!("The markdown text to render."))
  }

  fn outputTypes(&mut self) -> &Types {
    &STRING_TYPES
  }

  fn outputHelp(&mut self) -> OptionalString {
    *HELP_OUTPUT_EQUAL_INPUT
  }

  fn requiredVariables(&mut self) -> Option<&ExposedTypes> {
    self.requiring.clear();

    // Add UI.Parents to the list of required variables
    util::require_parents(&mut self.requiring);

    Some(&self.requiring)
  }

  fn warmup(&mut self, context: &Context) -> Result<(), &str> {
    self.parents.warmup(context);

    Ok(())
  }

  fn cleanup(&mut self, ctx: Option<&Context>) -> Result<(), &str> {
    self.parents.cleanup(ctx);

    Ok(())
  }

  fn activate(&mut self, _context: &Context, input: &Var) -> Result<Var, &str> {
    if let Some(ui) = util::get_current_parent_opt(self.parents.get())? {
      let text: &str = input.try_into()?;
      egui_commonmark::CommonMarkViewer::new(EguiId::new(self, 0)).show(ui, &mut self.cache, text);

      Ok(*input)
    } else {
      Err("No UI parent")
    }
  }
}
