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
  STRING_TYPES, SEQ_OF_ANY_TABLE_TYPES, INT_TYPES,
};
use shards::types::{Context, ExposedTypes, InstanceData, Type, Types, Var};

use grep_matcher::Matcher;
use grep_regex::RegexMatcherBuilder;
use grep_searcher::SearcherBuilder;
use grep_searcher::sinks::UTF8;

#[derive(shards::shard)]
#[shard_info("Codex.ApplyPatch", "Apply an OpenAI Codex patch to files with optional working directory.")]
struct ApplyPatchShard {
  #[shard_required]
  required: ExposedTypes,

  #[shard_param("WorkDir", "Working directory for resolving relative paths", [common_type::none, common_type::string_var])]
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

#[derive(shards::shard)]
#[shard_info("Codex.Grep", "Search for regex pattern in file(s) using ripgrep functionality")]
struct GrepShard {
  #[shard_required]
  required: ExposedTypes,

  #[shard_param("File", "File path to search in", [common_type::string, common_type::string_var])]
  file: ParamVar,

  #[shard_param("CaseInsensitive", "Enable case-insensitive search", [common_type::bool])]
  case_insensitive: ClonedVar,

  #[shard_param("LineNumbers", "Include line numbers in output", [common_type::bool])]
  line_numbers: ClonedVar,

  #[shard_param("BeforeContext", "Number of lines to show before each match", [common_type::int])]
  before_context: ClonedVar,

  #[shard_param("AfterContext", "Number of lines to show after each match", [common_type::int])]
  after_context: ClonedVar,

  #[shard_param("MultiLine", "Enable multi-line pattern matching", [common_type::bool])]
  multi_line: ClonedVar,

  #[shard_param("InvertMatch", "Show lines that don't match the pattern", [common_type::bool])]
  invert_match: ClonedVar,

  #[shard_param("MaxMatches", "Maximum number of matches to return (0 = unlimited)", [common_type::int])]
  max_matches: ClonedVar,

  output: AutoSeqVar,
}

impl Default for GrepShard {
  fn default() -> Self {
    Self {
      required: ExposedTypes::new(),
      file: ParamVar::default(),
      case_insensitive: false.into(),
      line_numbers: true.into(),
      before_context: 0i64.into(),
      after_context: 0i64.into(),
      multi_line: false.into(),
      invert_match: false.into(),
      max_matches: 0i64.into(),
      output: AutoSeqVar::new(),
    }
  }
}

#[shards::shard_impl]
impl Shard for GrepShard {
  fn input_types(&mut self) -> &Types {
    &STRING_TYPES
  }

  fn output_types(&mut self) -> &Types {
    &SEQ_OF_ANY_TABLE_TYPES
  }

  fn warmup(&mut self, ctx: &Context) -> Result<(), &str> {
    self.warmup_helper(ctx)?;
    Ok(())
  }

  fn cleanup(&mut self, ctx: Option<&Context>) -> Result<(), &str> {
    self.cleanup_helper(ctx)?;
    self.output.0.clear();
    Ok(())
  }

  fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
    self.compose_helper(data)?;
    Ok(self.output_types()[0])
  }

  fn activate(&mut self, _context: &Context, input: &Var) -> Result<Option<Var>, &str> {
    let pattern: &str = input.try_into()?;

    // Get file path
    let file_path: &str = self.file.get().as_ref().try_into().map_err(|_| {
      shlog_error!("File parameter must be a string");
      "File parameter must be a string"
    })?;

    // Get parameters
    let case_insensitive: bool = (&self.case_insensitive.0).try_into().map_err(|_| "CaseInsensitive must be a boolean")?;
    let line_numbers: bool = (&self.line_numbers.0).try_into().map_err(|_| "LineNumbers must be a boolean")?;
    let before_context: i64 = (&self.before_context.0).try_into().map_err(|_| "BeforeContext must be an integer")?;
    let after_context: i64 = (&self.after_context.0).try_into().map_err(|_| "AfterContext must be an integer")?;
    let multi_line: bool = (&self.multi_line.0).try_into().map_err(|_| "MultiLine must be a boolean")?;
    let invert_match: bool = (&self.invert_match.0).try_into().map_err(|_| "InvertMatch must be a boolean")?;
    let max_matches: i64 = (&self.max_matches.0).try_into().map_err(|_| "MaxMatches must be an integer")?;

    // Validate parameters
    if before_context < 0 {
      return Err("BeforeContext must be >= 0");
    }
    if after_context < 0 {
      return Err("AfterContext must be >= 0");
    }
    if max_matches < 0 {
      return Err("MaxMatches must be >= 0");
    }

    // Build the regex matcher
    let matcher = RegexMatcherBuilder::new()
      .case_insensitive(case_insensitive)
      .multi_line(multi_line)
      .build(pattern)
      .map_err(|e| {
        shlog_error!("Failed to build regex matcher: {}", e);
        "Failed to build regex matcher"
      })?;

    // Build the searcher
    let mut searcher = SearcherBuilder::new()
      .line_number(line_numbers)
      .before_context(before_context as usize)
      .after_context(after_context as usize)
      .multi_line(multi_line)
      .invert_match(invert_match)
      .build();

    // Clear previous results
    self.output.0.clear();

    // Track match count
    let mut match_count = 0i64;
    let max = if max_matches == 0 { i64::MAX } else { max_matches };

    // Search the file
    let file_path_clone = file_path.to_string();
    let result = searcher.search_path(
      &matcher,
      Path::new(file_path),
      UTF8(|lnum, line| {
        if match_count >= max {
          return Ok(false); // Stop searching
        }

        // Create a match table
        let mut match_table = AutoTableVar::new();

        if line_numbers {
          match_table.0.insert_fast_static("line_number", &(lnum as i64).into());
        }

        match_table.0.insert_fast_static("line", &Var::ephemeral_string(line));
        match_table.0.insert_fast_static("file", &Var::ephemeral_string(&file_path_clone));

        // Add to output
        self.output.0.push(&match_table.0.0);

        match_count += 1;
        Ok(true) // Continue searching
      }),
    );

    // Handle search errors
    if let Err(e) = result {
      shlog_error!("Grep search failed: {}", e);
      return Err("Grep search failed");
    }

    Ok(Some(self.output.0.0))
  }
}

#[no_mangle]
pub extern "C" fn shardsRegister_codex_rust(core: *mut shards::shardsc::SHCore) {
  unsafe {
    shards::core::Core = core;
  }

  register_shard::<ApplyPatchShard>();
  register_shard::<GrepShard>();
}
