/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2021 Fragcolor Pte. Ltd. */

use codex_apply_patch::apply_patch;

use shards::core::register_shard;
use shards::shard::Shard;
use shards::shlog_error;
use shards::types::{
  common_type, AutoSeqVar, AutoTableVar, ClonedVar, ParamVar, ANY_TABLE_TYPES, STRINGS_TYPES,
  STRING_TYPES,
};
use shards::types::{Context, ExposedTypes, InstanceData, Type, Types, Var};

#[derive(shards::shard)]
#[shard_info("Codex.ApplyPatch", "Apply an OpenAI Codex patch to files.")]
struct ApplyPatchShard {
  #[shard_required]
  required: ExposedTypes,

  output: AutoTableVar,
}

impl Default for ApplyPatchShard {
  fn default() -> Self {
    Self {
      required: ExposedTypes::new(),
      output: AutoTableVar::new(),
    }
  }
}

#[shards::shard_impl]
impl Shard for ApplyPatchShard {
  fn input_types(&mut self) -> &Types {
    &STRING_TYPES
  }

  fn output_types(&mut self) -> &Types {
    &ANY_TABLE_TYPES
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
    let patch: &str = input.try_into()?;
    let mut stdout = Vec::new();
    let mut stderr = Vec::new();
    apply_patch(patch, &mut stdout, &mut stderr).map_err(|e| {
      shlog_error!("Failed to apply patch: {}", e);
      "Failed to apply patch"
    })?;

    let stdout_str = String::from_utf8_lossy(&stdout);
    let stderr_str = String::from_utf8_lossy(&stderr);

    self
      .output
      .0
      .insert_fast_static("stdout", &Var::ephemeral_string(&stdout_str));
    self
      .output
      .0
      .insert_fast_static("stderr", &Var::ephemeral_string(&stderr_str));

    Ok(Some(self.output.0 .0))
  }
}

#[no_mangle]
pub extern "C" fn shardsRegister_codex_rust(core: *mut shards::shardsc::SHCore) {
  unsafe {
    shards::core::Core = core;
  }

  register_shard::<ApplyPatchShard>();
}
