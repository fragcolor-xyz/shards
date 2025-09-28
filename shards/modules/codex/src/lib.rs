/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2021 Fragcolor Pte. Ltd. */

use codex_apply_patch::{maybe_parse_apply_patch_verified, MaybeApplyPatchVerified, ApplyPatchFileChange};
use std::path::{Path, PathBuf};
use std::env;

use shards::core::register_shard;
use shards::shard::Shard;
use shards::shlog_error;
use shards::types::{
  common_type, AutoSeqVar, AutoTableVar, ClonedVar, ParamVar, ANY_TABLE_TYPES, STRINGS_TYPES,
  STRING_TYPES,
};
use shards::types::{Context, ExposedTypes, InstanceData, Type, Types, Var};

#[derive(shards::shard)]
#[shard_info("Codex.ApplyPatch", "Apply an OpenAI Codex patch to files with optional working directory.")]
struct ApplyPatchShard {
  #[shard_required]
  required: ExposedTypes,

  #[shard_param("WorkDir", "Working directory for resolving relative paths", [common_type::string, common_type::none])]
  work_dir: ParamVar,

  output: AutoTableVar,
}

impl Default for ApplyPatchShard {
  fn default() -> Self {
    Self {
      required: ExposedTypes::new(),
      work_dir: ParamVar::default(), // Default to None (use current directory)
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
    
    // Get working directory - either from parameter or current directory
    let work_dir = if self.work_dir.get().as_ref().is_none() {
      env::current_dir().map_err(|e| {
        shlog_error!("Failed to get current directory: {}", e);
        "Failed to get current directory"
      })?
    } else {
      let work_dir_str: &str = self.work_dir.get().as_ref().try_into().map_err(|_| {
        shlog_error!("WorkDir parameter must be a string");
        "WorkDir parameter must be a string"
      })?;
      PathBuf::from(work_dir_str)
    };

    // Prepare argv as if this was a direct apply_patch call
    let argv = vec!["apply_patch".to_string(), patch.to_string()];
    
    // Use the verified parser
    match maybe_parse_apply_patch_verified(&argv, &work_dir) {
      MaybeApplyPatchVerified::Body(action) => {
        // Successfully parsed and verified the patch
        let mut changes_table = AutoTableVar::new();
        
        // Convert changes to a more detailed format
        for (path, change) in action.changes() {
          let mut change_info = AutoTableVar::new();
          
          match change {
            ApplyPatchFileChange::Add { content } => {
              change_info.0.insert_fast_static("type", &Var::ephemeral_string("add"));
              change_info.0.insert_fast_static("content", &Var::ephemeral_string(content));
            }
            ApplyPatchFileChange::Delete { content } => {
              change_info.0.insert_fast_static("type", &Var::ephemeral_string("delete"));
              change_info.0.insert_fast_static("content", &Var::ephemeral_string(content));
            }
            ApplyPatchFileChange::Update { unified_diff, move_path, new_content } => {
              change_info.0.insert_fast_static("type", &Var::ephemeral_string("update"));
              change_info.0.insert_fast_static("unified_diff", &Var::ephemeral_string(unified_diff));
              change_info.0.insert_fast_static("new_content", &Var::ephemeral_string(new_content));
              if let Some(move_path) = move_path {
                change_info.0.insert_fast_static("move_to", &Var::ephemeral_string(&move_path.display().to_string()));
              }
            }
          }
          
          let path_str = path.display().to_string();
          changes_table.0.insert_fast_static(&path_str, &change_info.0.0);
        }

        // Set output fields
        self.output.0.insert_fast_static("success", &true.into());
        self.output.0.insert_fast_static("changes", &changes_table.0.0);
        self.output.0.insert_fast_static("working_directory", &Var::ephemeral_string(&action.cwd.display().to_string()));
        self.output.0.insert_fast_static("patch", &Var::ephemeral_string(&action.patch));
      }
      
      MaybeApplyPatchVerified::CorrectnessError(err) => {
        shlog_error!("Patch correctness error: {}", err);
        self.output.0.insert_fast_static("success", &false.into());
        self.output.0.insert_fast_static("error", &Var::ephemeral_string(&err.to_string()));
        self.output.0.insert_fast_static("error_type", &Var::ephemeral_string("correctness"));
      }
      
      MaybeApplyPatchVerified::ShellParseError(err) => {
        shlog_error!("Shell parse error: {:?}", err);
        self.output.0.insert_fast_static("success", &false.into());
        self.output.0.insert_fast_static("error", &Var::ephemeral_string(&format!("{:?}", err)));
        self.output.0.insert_fast_static("error_type", &Var::ephemeral_string("shell_parse"));
      }
      
      MaybeApplyPatchVerified::NotApplyPatch => {
        shlog_error!("Input does not appear to be a valid apply_patch command");
        self.output.0.insert_fast_static("success", &false.into());
        self.output.0.insert_fast_static("error", &Var::ephemeral_string("Input does not appear to be a valid apply_patch command"));
        self.output.0.insert_fast_static("error_type", &Var::ephemeral_string("not_apply_patch"));
      }
    }

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
