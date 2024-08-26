/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

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
use shards::types::InstanceData;
use shards::shard::Shard;
use shards::types::Type;

const THEME_LIGHT: &str = "Solarized (light)";
const THEME_DARK: &str = "Solarized (dark)";

#[derive(shards::shard)]
#[shard_info("UI.MarkdownViewer", "A markdown viewer.")]
pub struct MarkdownViewerShard {
  #[shard_required]
  requiring: ExposedTypes,
  #[shard_warmup]
  parents: ParamVar,

  cache: egui_commonmark::CommonMarkCache,
}

impl Default for MarkdownViewerShard {
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

#[shards::shard_impl]
impl Shard for MarkdownViewerShard {
  fn input_types(&mut self) -> &Types {
    &STRING_TYPES
  }

  fn output_types(&mut self) -> &Types {
    &STRING_TYPES
  }

  fn warmup(&mut self, ctx: &Context) -> Result<(), &str> {
    self.warmup_helper(ctx)?;
    Ok(())
  }

  fn cleanup(&mut self, ctx: Option<&Context>) -> Result<(), &str> {
    self.cleanup_helper(ctx)?;
    Ok(())
  }

  fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
    self.compose_helper(data)?;
    Ok(self.output_types()[0])
  }

  fn activate(&mut self, _context: &Context, input: &Var) -> Result<Option<Var>, &str> {
    if let Some(ui) = util::get_current_parent_opt(self.parents.get())? {
      let text: &str = input.try_into()?;
      egui_commonmark::CommonMarkViewer::new(EguiId::new(self, 0))
        .indentation_spaces(2)
        .syntax_theme_dark(THEME_DARK)
        .syntax_theme_light(THEME_LIGHT)
        .show(ui, &mut self.cache, text);
      Ok(None)
    } else {
      Err("No UI parent")
    }
  }
}
