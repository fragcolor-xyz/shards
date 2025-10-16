/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2024 Fragcolor Pte. Ltd. */

use std::path::{Path, PathBuf};
use std::env;
use std::fs;
use std::collections::hash_map::DefaultHasher;
use std::hash::{Hash, Hasher};
use std::io::Read;

use shards::core::register_shard;
use shards::shard::Shard;
use shards::shlog_error;
use shards::types::{
  common_type, AutoSeqVar, AutoTableVar, ClonedVar, ParamVar,
  STRING_TYPES, SEQ_OF_ANY_TABLE_TYPES, NONE_TYPES,
};
use shards::types::{Context, ExposedTypes, InstanceData, Type, Types, Var};

use grep_regex::RegexMatcherBuilder;
use grep_searcher::{SearcherBuilder, Searcher, Sink, SinkMatch, SinkContext};

// ============================================================================
// Common Utilities
// ============================================================================

/// Resolve a path relative to shell CWD, with tilde expansion
fn resolve_path(path: &str, shell_cwd: &Path) -> PathBuf {
  if path.starts_with("~/") {
    // Expand home directory
    if let Ok(home) = env::var("HOME") {
      PathBuf::from(home).join(&path[2..])
    } else {
      PathBuf::from(path)
    }
  } else if Path::new(path).is_absolute() {
    PathBuf::from(path)
  } else {
    // Relative to shell CWD
    shell_cwd.join(path)
  }
}

/// Create a backup of the file if it exists
fn create_backup(abs_path: &Path) -> Result<(), String> {
  if !abs_path.exists() {
    return Ok(()); // No backup needed for new file
  }

  let backup_dir = Path::new(".edits_backup");
  fs::create_dir_all(backup_dir).map_err(|e| format!("Failed to create backup directory: {}", e))?;

  // Use hash of absolute path for unique backup filename
  let hash = calculate_hash(&abs_path.to_string_lossy());
  let backup_path = backup_dir.join(format!("{}.bak", hash));

  fs::copy(abs_path, backup_path).map_err(|e| format!("Failed to create backup: {}", e))?;
  Ok(())
}

fn calculate_hash(s: &str) -> u64 {
  let mut hasher = DefaultHasher::new();
  s.hash(&mut hasher);
  hasher.finish()
}

/// Check if a path is inside a git repository
fn is_git_repo(path: &Path) -> bool {
  use std::process::Command;

  let dir = if path.is_file() {
    path.parent().unwrap_or(path)
  } else {
    path
  };

  Command::new("git")
    .args(&["rev-parse", "--is-inside-work-tree"])
    .current_dir(dir)
    .output()
    .map(|o| o.status.success())
    .unwrap_or(false)
}

/// Auto-commit a file to git
fn git_commit_file(path: &Path, operation: &str) -> Result<(), String> {
  use std::process::Command;

  let dir = path.parent().unwrap_or(path);
  let filename = path.file_name()
    .and_then(|n| n.to_str())
    .unwrap_or("unknown");

  // git add
  let add_result = Command::new("git")
    .args(&["add", filename])
    .current_dir(dir)
    .output()
    .map_err(|e| format!("Git add failed: {}", e))?;

  if !add_result.status.success() {
    shlog_error!("Warning: git add failed");
  }

  // git commit
  let msg = format!("{} file {}", operation, filename);
  let commit_result = Command::new("git")
    .args(&["commit", "-m", &msg])
    .current_dir(dir)
    .output()
    .map_err(|e| format!("Git commit failed: {}", e))?;

  if !commit_result.status.success() {
    shlog_error!("Warning: git commit failed");
  }

  Ok(())
}

/// Check if file is binary
fn is_binary_file(path: &Path) -> Result<bool, String> {
  let mut buffer = [0u8; 8000];
  let mut file = fs::File::open(path).map_err(|e| format!("Failed to open file: {}", e))?;

  let n = file.read(&mut buffer).map_err(|e| format!("Failed to read file: {}", e))?;
  if n == 0 {
    return Ok(false); // Empty file
  }

  // Check for null bytes (common indicator of binary)
  Ok(buffer[..n].contains(&0))
}

// ============================================================================
// Grep Shard
// ============================================================================

#[derive(shards::shard)]
#[shard_info("Grep", "Search for regex pattern in file(s) using ripgrep functionality")]
struct GrepShard {
  #[shard_required]
  required: ExposedTypes,

  #[shard_param("File", "File path to search in", [common_type::string, common_type::string_var])]
  file: ParamVar,

  #[shard_param("CaseInsensitive", "Enable case-insensitive search", [common_type::bool, common_type::bool_var])]
  case_insensitive: ParamVar,

  #[shard_param("LineNumbers", "Include line numbers in output", [common_type::bool, common_type::bool_var])]
  line_numbers: ParamVar,

  #[shard_param("BeforeContext", "Number of lines to show before each match", [common_type::int, common_type::int_var])]
  before_context: ParamVar,

  #[shard_param("AfterContext", "Number of lines to show after each match", [common_type::int, common_type::int_var])]
  after_context: ParamVar,

  #[shard_param("MultiLine", "Enable multi-line pattern matching", [common_type::bool, common_type::bool_var])]
  multi_line: ParamVar,

  #[shard_param("InvertMatch", "Show lines that don't match the pattern", [common_type::bool, common_type::bool_var])]
  invert_match: ParamVar,

  #[shard_param("MaxMatches", "Maximum number of matches to return (0 = unlimited)", [common_type::int, common_type::int_var])]
  max_matches: ParamVar,

  output: AutoSeqVar,
}

impl Default for GrepShard {
  fn default() -> Self {
    Self {
      required: ExposedTypes::new(),
      file: ParamVar::default(),
      case_insensitive: ParamVar::new(false.into()),
      line_numbers: ParamVar::new(true.into()),
      before_context: ParamVar::new(0i64.into()),
      after_context: ParamVar::new(0i64.into()),
      multi_line: ParamVar::new(false.into()),
      invert_match: ParamVar::new(false.into()),
      max_matches: ParamVar::new(0i64.into()),
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
    let case_insensitive: bool = self.case_insensitive.get().as_ref().try_into().map_err(|_| "CaseInsensitive must be a boolean")?;
    let line_numbers: bool = self.line_numbers.get().as_ref().try_into().map_err(|_| "LineNumbers must be a boolean")?;
    let before_context: i64 = self.before_context.get().as_ref().try_into().map_err(|_| "BeforeContext must be an integer")?;
    let after_context: i64 = self.after_context.get().as_ref().try_into().map_err(|_| "AfterContext must be an integer")?;
    let multi_line: bool = self.multi_line.get().as_ref().try_into().map_err(|_| "MultiLine must be a boolean")?;
    let invert_match: bool = self.invert_match.get().as_ref().try_into().map_err(|_| "InvertMatch must be a boolean")?;
    let max_matches: i64 = self.max_matches.get().as_ref().try_into().map_err(|_| "MaxMatches must be an integer")?;

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

    // Custom sink to properly count only matches, not context lines
    struct GrepSinkImpl {
      output: AutoSeqVar,
      file_path: String,
      line_numbers: bool,
      match_count: i64,
      max_matches: i64,
    }

    impl Sink for GrepSinkImpl {
      type Error = std::io::Error;

      fn matched(&mut self, _searcher: &Searcher, mat: &SinkMatch<'_>) -> Result<bool, Self::Error> {
        if self.match_count >= self.max_matches {
          return Ok(false); // Stop searching
        }

        let line = String::from_utf8_lossy(mat.bytes());
        let mut match_table = AutoTableVar::new();

        if self.line_numbers {
          if let Some(lnum) = mat.line_number() {
            match_table.0.insert_fast_static("line_number", &(lnum as i64).into());
          }
        }

        match_table.0.insert_fast_static("line", &Var::ephemeral_string(&line));
        match_table.0.insert_fast_static("file", &Var::ephemeral_string(&self.file_path));
        match_table.0.insert_fast_static("is_match", &true.into());

        self.output.0.push(&match_table.0.0);
        self.match_count += 1;

        Ok(true) // Continue searching
      }

      fn context(&mut self, _searcher: &Searcher, context: &SinkContext<'_>) -> Result<bool, Self::Error> {
        // Context lines don't count toward match limit
        let line = String::from_utf8_lossy(context.bytes());
        let mut context_table = AutoTableVar::new();

        if self.line_numbers {
          if let Some(lnum) = context.line_number() {
            context_table.0.insert_fast_static("line_number", &(lnum as i64).into());
          }
        }

        context_table.0.insert_fast_static("line", &Var::ephemeral_string(&line));
        context_table.0.insert_fast_static("file", &Var::ephemeral_string(&self.file_path));
        context_table.0.insert_fast_static("is_match", &false.into());

        self.output.0.push(&context_table.0.0);

        Ok(true) // Continue searching
      }
    }

    let max = if max_matches == 0 { i64::MAX } else { max_matches };
    let mut sink = GrepSinkImpl {
      output: AutoSeqVar::new(),
      file_path: file_path.to_string(),
      line_numbers,
      match_count: 0,
      max_matches: max,
    };

    // Search the file
    let result = searcher.search_path(&matcher, Path::new(file_path), &mut sink);

    // Handle search errors
    if let Err(e) = result {
      shlog_error!("Grep search failed: {}", e);
      return Err("Grep search failed");
    }

    // Transfer results to self.output
    self.output = sink.output;

    Ok(Some(self.output.0.0))
  }
}

// ============================================================================
// ViewFile Shard
// ============================================================================

#[derive(shards::shard)]
#[shard_info("FileOps.ViewFile", "View file contents with line numbers or list directory contents")]
struct ViewFileShard {
  #[shard_required]
  required: ExposedTypes,

  #[shard_param("WorkDir", "Working directory for resolving relative paths", [common_type::none, common_type::string, common_type::string_var])]
  work_dir: ParamVar,

  #[shard_param("StartLine", "Starting line number (1-indexed, default: 1)", [common_type::none, common_type::int, common_type::int_var])]
  start_line: ParamVar,

  #[shard_param("EndLine", "Ending line number (use -1 for end of file, default: -1)", [common_type::none, common_type::int, common_type::int_var])]
  end_line: ParamVar,

  output: ClonedVar,
}

impl Default for ViewFileShard {
  fn default() -> Self {
    Self {
      required: ExposedTypes::new(),
      work_dir: ParamVar::default(),
      start_line: ParamVar::new(1i64.into()),
      end_line: ParamVar::new((-1i64).into()),
      output: ClonedVar::default(),
    }
  }
}

#[shards::shard_impl]
impl Shard for ViewFileShard {
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
    self.output = ClonedVar::default();
    Ok(())
  }

  fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
    self.compose_helper(data)?;
    Ok(self.output_types()[0])
  }

  fn activate(&mut self, _context: &Context, input: &Var) -> Result<Option<Var>, &str> {
    // Get path from input
    let path_str: &str = input.try_into()?;

    // Get working directory
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

    // Extract line range parameters early to avoid borrowing conflicts
    let start_line_val: i64 = self.start_line.get().as_ref().try_into()
      .map_err(|_| "StartLine must be an integer")?;
    let end_line_val: i64 = self.end_line.get().as_ref().try_into()
      .map_err(|_| "EndLine must be an integer")?;

    // Resolve path
    let abs_path = resolve_path(path_str, &work_dir);

    if !abs_path.exists() {
      shlog_error!("File not found: {}", abs_path.display());
      return Err("File not found");
    }

    // Call view methods with extracted parameters
    // Use a block to ensure borrows end before assigning to self.output
    let result = {
      if abs_path.is_file() {
        Self::view_file_contents_static(&abs_path, start_line_val, end_line_val)?
      } else if abs_path.is_dir() {
        Self::view_directory_static(&abs_path)?
      } else {
        return Err("Path is neither file nor directory");
      }
    };

    self.output = Var::ephemeral_string(&result).into();
    Ok(Some(self.output.0))
  }
}

impl ViewFileShard {
  fn view_file_contents_static(abs_path: &Path, start_line_val: i64, end_line_val: i64) -> Result<String, &'static str> {
    // Check if binary
    if is_binary_file(abs_path).map_err(|_| "Failed to check if file is binary")? {
      shlog_error!("Cannot view binary file: {}", abs_path.display());
      return Err("Cannot view binary file");
    }

    let content = fs::read_to_string(abs_path).map_err(|e| {
      shlog_error!("Failed to read file: {}", e);
      "Failed to read file"
    })?;

    let lines: Vec<&str> = content.lines().collect();
    let total_lines = lines.len();

    // Determine range (convert to 0-indexed)
    let start = (start_line_val.max(1) as usize).saturating_sub(1);
    let end = if end_line_val == -1 {
      total_lines
    } else if end_line_val > 0 {
      (end_line_val as usize).min(total_lines)
    } else {
      shlog_error!("Invalid end line number: {}", end_line_val);
      return Err("Invalid end line number");
    };

    // Validate range
    if start >= total_lines && total_lines > 0 {
      shlog_error!("Start line {} exceeds file length {}", start + 1, total_lines);
      return Err("Start line exceeds file length");
    }

    // Format with line numbers
    let numbered_lines: Vec<String> = lines[start..end]
      .iter()
      .enumerate()
      .map(|(i, line)| format!("{:>6}\t{}", start + i + 1, line))
      .collect();

    Ok(numbered_lines.join("\n"))
  }

  fn view_directory_static(abs_path: &Path) -> Result<String, &'static str> {
    use std::collections::BTreeSet;
    use walkdir::WalkDir;

    let mut files = BTreeSet::new();

    for entry in WalkDir::new(abs_path)
      .max_depth(2)
      .into_iter()
      .filter_entry(|e| {
        // Skip hidden files/dirs
        !e.file_name()
          .to_str()
          .map(|s| s.starts_with('.'))
          .unwrap_or(false)
      })
    {
      let entry = entry.map_err(|e| {
        shlog_error!("Directory walk error: {}", e);
        "Directory walk error"
      })?;

      if entry.file_type().is_file() {
        if let Ok(rel_path) = entry.path().strip_prefix(abs_path) {
          files.insert(rel_path.display().to_string());
        }
      }
    }

    Ok(files.into_iter().collect::<Vec<_>>().join("\n"))
  }
}

// ============================================================================
// CreateFile Shard
// ============================================================================

#[derive(shards::shard)]
#[shard_info("FileOps.CreateFile", "Create a new file or overwrite existing file with content")]
struct CreateFileShard {
  #[shard_required]
  required: ExposedTypes,

  #[shard_param("Path", "File path to create", [common_type::string, common_type::string_var])]
  path: ParamVar,

  #[shard_param("WorkDir", "Working directory for resolving relative paths", [common_type::none, common_type::string, common_type::string_var])]
  work_dir: ParamVar,

  #[shard_param("EnableGitCommit", "Auto-commit to git if in repository", [common_type::bool, common_type::bool_var])]
  enable_git_commit: ParamVar,

  output: ClonedVar,
}

impl Default for CreateFileShard {
  fn default() -> Self {
    Self {
      required: ExposedTypes::new(),
      path: ParamVar::default(),
      work_dir: ParamVar::default(),
      enable_git_commit: ParamVar::new(false.into()),
      output: ClonedVar::default(),
    }
  }
}

#[shards::shard_impl]
impl Shard for CreateFileShard {
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
    self.output = ClonedVar::default();
    Ok(())
  }

  fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
    self.compose_helper(data)?;
    Ok(self.output_types()[0])
  }

  fn activate(&mut self, _context: &Context, input: &Var) -> Result<Option<Var>, &str> {
    let content: &str = input.try_into()?;

    // Get path parameter
    let path_str: &str = self.path.get().as_ref().try_into().map_err(|_| {
      shlog_error!("Path parameter must be a string");
      "Path parameter must be a string"
    })?;

    // Get working directory
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

    // Resolve path
    let abs_path = resolve_path(path_str, &work_dir);

    // Create backup if file exists
    create_backup(&abs_path).map_err(|e| {
      shlog_error!("Backup failed: {}", e);
      "Failed to create backup"
    })?;

    // Create parent directories
    if let Some(parent) = abs_path.parent() {
      fs::create_dir_all(parent).map_err(|e| {
        shlog_error!("Failed to create parent directories: {}", e);
        "Failed to create parent directories"
      })?;
    }

    // Write file
    fs::write(&abs_path, content).map_err(|e| {
      shlog_error!("Failed to write file: {}", e);
      "Failed to write file"
    })?;

    // Git commit if enabled and in repo
    let enable_git: bool = self.enable_git_commit.get().as_ref().try_into()
      .map_err(|_| "EnableGitCommit must be a boolean")?;

    if enable_git && is_git_repo(&abs_path) {
      let _ = git_commit_file(&abs_path, "Creating");
    }

    let size = content.len();
    let result = format!("File created: {} ({} bytes)", abs_path.display(), size);
    self.output = Var::ephemeral_string(&result).into();
    Ok(Some(self.output.0))
  }
}

// ============================================================================
// ReplaceStringUnique Shard
// ============================================================================

#[derive(shards::shard)]
#[shard_info("FileOps.ReplaceStringUnique", "Replace a string that appears exactly once in file")]
struct ReplaceStringUniqueShard {
  #[shard_required]
  required: ExposedTypes,

  #[shard_param("OldStr", "String to find (must appear exactly once)", [common_type::string, common_type::string_var])]
  old_str: ParamVar,

  #[shard_param("NewStr", "Replacement string", [common_type::string, common_type::string_var])]
  new_str: ParamVar,

  #[shard_param("WorkDir", "Working directory for resolving relative paths", [common_type::none, common_type::string, common_type::string_var])]
  work_dir: ParamVar,

  #[shard_param("EnableGitCommit", "Auto-commit to git if in repository", [common_type::bool, common_type::bool_var])]
  enable_git_commit: ParamVar,

  output: ClonedVar,
}

impl Default for ReplaceStringUniqueShard {
  fn default() -> Self {
    Self {
      required: ExposedTypes::new(),
      old_str: ParamVar::default(),
      new_str: ParamVar::default(),
      work_dir: ParamVar::default(),
      enable_git_commit: ParamVar::new(false.into()),
      output: ClonedVar::default(),
    }
  }
}

#[shards::shard_impl]
impl Shard for ReplaceStringUniqueShard {
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
    self.output = ClonedVar::default();
    Ok(())
  }

  fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
    self.compose_helper(data)?;
    Ok(self.output_types()[0])
  }

  fn activate(&mut self, _context: &Context, input: &Var) -> Result<Option<Var>, &str> {
    // Get path from input
    let path_str: &str = input.try_into()?;

    // Get parameters
    let old_str: &str = self.old_str.get().as_ref().try_into().map_err(|_| "OldStr must be a string")?;
    let new_str: &str = self.new_str.get().as_ref().try_into().map_err(|_| "NewStr must be a string")?;

    if old_str.is_empty() {
      shlog_error!("OldStr cannot be empty");
      return Err("OldStr cannot be empty");
    }

    // Get working directory
    let work_dir = if self.work_dir.get().as_ref().is_none() {
      env::current_dir().map_err(|_| "Failed to get current directory")?
    } else {
      let work_dir_str: &str = self.work_dir.get().as_ref().try_into()
        .map_err(|_| "WorkDir must be a string")?;
      PathBuf::from(work_dir_str)
    };

    // Resolve path
    let abs_path = resolve_path(path_str, &work_dir);

    if !abs_path.exists() {
      shlog_error!("File not found: {}", abs_path.display());
      return Err("File not found");
    }

    let content = fs::read_to_string(&abs_path).map_err(|e| {
      shlog_error!("Failed to read file: {}", e);
      "Failed to read file"
    })?;

    let count = content.matches(old_str).count();

    if count != 1 {
      shlog_error!("Found {} occurrences (expected exactly 1)", count);
      return Err("String does not appear exactly once");
    }

    // Create backup
    create_backup(&abs_path).map_err(|e| {
      shlog_error!("Backup failed: {}", e);
      "Failed to create backup"
    })?;

    // Perform replacement
    let new_content = content.replace(old_str, new_str);
    fs::write(&abs_path, new_content).map_err(|e| {
      shlog_error!("Failed to write file: {}", e);
      "Failed to write file"
    })?;

    // Git commit if enabled
    let enable_git: bool = self.enable_git_commit.get().as_ref().try_into()
      .map_err(|_| "EnableGitCommit must be a boolean")?;

    if enable_git && is_git_repo(&abs_path) {
      let _ = git_commit_file(&abs_path, "Editing");
    }

    let result = format!("Replaced 1 occurrence in {}", abs_path.display());
    self.output = Var::ephemeral_string(&result).into();
    Ok(Some(self.output.0))
  }
}

// ============================================================================
// ReplaceStringFirst Shard
// ============================================================================

#[derive(shards::shard)]
#[shard_info("FileOps.ReplaceStringFirst", "Replace the first occurrence of a string in file")]
struct ReplaceStringFirstShard {
  #[shard_required]
  required: ExposedTypes,

  #[shard_param("OldStr", "String to find", [common_type::string, common_type::string_var])]
  old_str: ParamVar,

  #[shard_param("NewStr", "Replacement string", [common_type::string, common_type::string_var])]
  new_str: ParamVar,

  #[shard_param("WorkDir", "Working directory for resolving relative paths", [common_type::none, common_type::string, common_type::string_var])]
  work_dir: ParamVar,

  #[shard_param("EnableGitCommit", "Auto-commit to git if in repository", [common_type::bool, common_type::bool_var])]
  enable_git_commit: ParamVar,

  output: ClonedVar,
}

impl Default for ReplaceStringFirstShard {
  fn default() -> Self {
    Self {
      required: ExposedTypes::new(),
      old_str: ParamVar::default(),
      new_str: ParamVar::default(),
      work_dir: ParamVar::default(),
      enable_git_commit: ParamVar::new(false.into()),
      output: ClonedVar::default(),
    }
  }
}

#[shards::shard_impl]
impl Shard for ReplaceStringFirstShard {
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
    self.output = ClonedVar::default();
    Ok(())
  }

  fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
    self.compose_helper(data)?;
    Ok(self.output_types()[0])
  }

  fn activate(&mut self, _context: &Context, input: &Var) -> Result<Option<Var>, &str> {
    // Get path from input
    let path_str: &str = input.try_into()?;

    // Get parameters
    let old_str: &str = self.old_str.get().as_ref().try_into().map_err(|_| "OldStr must be a string")?;
    let new_str: &str = self.new_str.get().as_ref().try_into().map_err(|_| "NewStr must be a string")?;

    if old_str.is_empty() {
      shlog_error!("OldStr cannot be empty");
      return Err("OldStr cannot be empty");
    }

    // Get working directory
    let work_dir = if self.work_dir.get().as_ref().is_none() {
      env::current_dir().map_err(|_| "Failed to get current directory")?
    } else {
      let work_dir_str: &str = self.work_dir.get().as_ref().try_into()
        .map_err(|_| "WorkDir must be a string")?;
      PathBuf::from(work_dir_str)
    };

    // Resolve path
    let abs_path = resolve_path(path_str, &work_dir);

    if !abs_path.exists() {
      shlog_error!("File not found: {}", abs_path.display());
      return Err("File not found");
    }

    let content = fs::read_to_string(&abs_path).map_err(|e| {
      shlog_error!("Failed to read file: {}", e);
      "Failed to read file"
    })?;

    let total_count = content.matches(old_str).count();

    if total_count == 0 {
      shlog_error!("String not found in file");
      return Err("String not found");
    }

    // Create backup
    create_backup(&abs_path).map_err(|e| {
      shlog_error!("Backup failed: {}", e);
      "Failed to create backup"
    })?;

    // Replace only first occurrence
    let new_content = content.replacen(old_str, new_str, 1);
    fs::write(&abs_path, new_content).map_err(|e| {
      shlog_error!("Failed to write file: {}", e);
      "Failed to write file"
    })?;

    // Git commit if enabled
    let enable_git: bool = self.enable_git_commit.get().as_ref().try_into()
      .map_err(|_| "EnableGitCommit must be a boolean")?;

    if enable_git && is_git_repo(&abs_path) {
      let _ = git_commit_file(&abs_path, "Editing");
    }

    let matches_text = if total_count == 1 { "match" } else { "matches" };
    let result = format!(
      "Replaced first occurrence in {} (found {} total {})",
      abs_path.display(),
      total_count,
      matches_text
    );
    self.output = Var::ephemeral_string(&result).into();
    Ok(Some(self.output.0))
  }
}

// ============================================================================
// ReplaceStringAll Shard
// ============================================================================

#[derive(shards::shard)]
#[shard_info("FileOps.ReplaceStringAll", "Replace all occurrences of a string in file")]
struct ReplaceStringAllShard {
  #[shard_required]
  required: ExposedTypes,

  #[shard_param("OldStr", "String to find", [common_type::string, common_type::string_var])]
  old_str: ParamVar,

  #[shard_param("NewStr", "Replacement string", [common_type::string, common_type::string_var])]
  new_str: ParamVar,

  #[shard_param("WorkDir", "Working directory for resolving relative paths", [common_type::none, common_type::string, common_type::string_var])]
  work_dir: ParamVar,

  #[shard_param("EnableGitCommit", "Auto-commit to git if in repository", [common_type::bool, common_type::bool_var])]
  enable_git_commit: ParamVar,

  output: ClonedVar,
}

impl Default for ReplaceStringAllShard {
  fn default() -> Self {
    Self {
      required: ExposedTypes::new(),
      old_str: ParamVar::default(),
      new_str: ParamVar::default(),
      work_dir: ParamVar::default(),
      enable_git_commit: ParamVar::new(false.into()),
      output: ClonedVar::default(),
    }
  }
}

#[shards::shard_impl]
impl Shard for ReplaceStringAllShard {
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
    self.output = ClonedVar::default();
    Ok(())
  }

  fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
    self.compose_helper(data)?;
    Ok(self.output_types()[0])
  }

  fn activate(&mut self, _context: &Context, input: &Var) -> Result<Option<Var>, &str> {
    // Get path from input
    let path_str: &str = input.try_into()?;

    // Get parameters
    let old_str: &str = self.old_str.get().as_ref().try_into().map_err(|_| "OldStr must be a string")?;
    let new_str: &str = self.new_str.get().as_ref().try_into().map_err(|_| "NewStr must be a string")?;

    if old_str.is_empty() {
      shlog_error!("OldStr cannot be empty");
      return Err("OldStr cannot be empty");
    }

    // Get working directory
    let work_dir = if self.work_dir.get().as_ref().is_none() {
      env::current_dir().map_err(|_| "Failed to get current directory")?
    } else {
      let work_dir_str: &str = self.work_dir.get().as_ref().try_into()
        .map_err(|_| "WorkDir must be a string")?;
      PathBuf::from(work_dir_str)
    };

    // Resolve path
    let abs_path = resolve_path(path_str, &work_dir);

    if !abs_path.exists() {
      shlog_error!("File not found: {}", abs_path.display());
      return Err("File not found");
    }

    let content = fs::read_to_string(&abs_path).map_err(|e| {
      shlog_error!("Failed to read file: {}", e);
      "Failed to read file"
    })?;

    let count = content.matches(old_str).count();

    if count == 0 {
      shlog_error!("String not found in file");
      return Err("String not found");
    }

    // Create backup
    create_backup(&abs_path).map_err(|e| {
      shlog_error!("Backup failed: {}", e);
      "Failed to create backup"
    })?;

    // Replace all occurrences
    let new_content = content.replace(old_str, new_str);
    fs::write(&abs_path, new_content).map_err(|e| {
      shlog_error!("Failed to write file: {}", e);
      "Failed to write file"
    })?;

    // Git commit if enabled
    let enable_git: bool = self.enable_git_commit.get().as_ref().try_into()
      .map_err(|_| "EnableGitCommit must be a boolean")?;

    if enable_git && is_git_repo(&abs_path) {
      let _ = git_commit_file(&abs_path, "Editing");
    }

    let occurrences_text = if count == 1 { "occurrence" } else { "occurrences" };
    let result = format!(
      "Replaced {} {} in {}",
      count,
      occurrences_text,
      abs_path.display()
    );
    self.output = Var::ephemeral_string(&result).into();
    Ok(Some(self.output.0))
  }
}

// ============================================================================
// InsertAtLine Shard
// ============================================================================

#[derive(shards::shard)]
#[shard_info("FileOps.InsertAtLine", "Insert text at a specific line number in file")]
struct InsertAtLineShard {
  #[shard_required]
  required: ExposedTypes,

  #[shard_param("Path", "File path", [common_type::string, common_type::string_var])]
  path: ParamVar,

  #[shard_param("LineNumber", "Line number to insert after (0 = beginning)", [common_type::int, common_type::int_var])]
  line_number: ParamVar,

  #[shard_param("WorkDir", "Working directory for resolving relative paths", [common_type::none, common_type::string, common_type::string_var])]
  work_dir: ParamVar,

  #[shard_param("EnableGitCommit", "Auto-commit to git if in repository", [common_type::bool, common_type::bool_var])]
  enable_git_commit: ParamVar,

  output: ClonedVar,
}

impl Default for InsertAtLineShard {
  fn default() -> Self {
    Self {
      required: ExposedTypes::new(),
      path: ParamVar::default(),
      line_number: ParamVar::default(),
      work_dir: ParamVar::default(),
      enable_git_commit: ParamVar::new(false.into()),
      output: ClonedVar::default(),
    }
  }
}

#[shards::shard_impl]
impl Shard for InsertAtLineShard {
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
    self.output = ClonedVar::default();
    Ok(())
  }

  fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
    self.compose_helper(data)?;
    Ok(self.output_types()[0])
  }

  fn activate(&mut self, _context: &Context, input: &Var) -> Result<Option<Var>, &str> {
    let content: &str = input.try_into()?;

    // Get parameters
    let path_str: &str = self.path.get().as_ref().try_into().map_err(|_| "Path must be a string")?;
    let line_number: i64 = self.line_number.get().as_ref().try_into().map_err(|_| "LineNumber must be an integer")?;

    if line_number < 0 {
      shlog_error!("LineNumber must be >= 0");
      return Err("LineNumber must be >= 0");
    }

    // Get working directory
    let work_dir = if self.work_dir.get().as_ref().is_none() {
      env::current_dir().map_err(|_| "Failed to get current directory")?
    } else {
      let work_dir_str: &str = self.work_dir.get().as_ref().try_into()
        .map_err(|_| "WorkDir must be a string")?;
      PathBuf::from(work_dir_str)
    };

    // Resolve path
    let abs_path = resolve_path(path_str, &work_dir);

    if !abs_path.exists() {
      shlog_error!("File not found: {}", abs_path.display());
      return Err("File not found");
    }

    let file_content = fs::read_to_string(&abs_path).map_err(|e| {
      shlog_error!("Failed to read file: {}", e);
      "Failed to read file"
    })?;

    let mut lines: Vec<String> = file_content.lines().map(|s| s.to_string()).collect();
    let total_lines = lines.len();

    // Validate line number
    let line_num = line_number as usize;
    if line_num > total_lines {
      shlog_error!("Invalid line number: {} (file has {} lines)", line_num, total_lines);
      return Err("Invalid line number");
    }

    // Create backup
    create_backup(&abs_path).map_err(|e| {
      shlog_error!("Backup failed: {}", e);
      "Failed to create backup"
    })?;

    // Insert content (line_number is where to insert after)
    // line_number = 0 means insert at beginning
    // line_number = N means insert after line N
    lines.insert(line_num, content.to_string());

    // Write back
    let new_content = lines.join("\n") + "\n";
    fs::write(&abs_path, new_content).map_err(|e| {
      shlog_error!("Failed to write file: {}", e);
      "Failed to write file"
    })?;

    // Git commit if enabled
    let enable_git: bool = self.enable_git_commit.get().as_ref().try_into()
      .map_err(|_| "EnableGitCommit must be a boolean")?;

    if enable_git && is_git_repo(&abs_path) {
      let _ = git_commit_file(&abs_path, "Inserting into");
    }

    let result = format!(
      "Inserted content at line {} in {}",
      line_num,
      abs_path.display()
    );
    self.output = Var::ephemeral_string(&result).into();
    Ok(Some(self.output.0))
  }
}

// ============================================================================
// Module Registration
// ============================================================================

#[no_mangle]
pub extern "C" fn shardsRegister_fileops_rust(core: *mut shards::shardsc::SHCore) {
  unsafe {
    shards::core::Core = core;
  }

  register_shard::<GrepShard>();
  register_shard::<ViewFileShard>();
  register_shard::<CreateFileShard>();
  register_shard::<ReplaceStringUniqueShard>();
  register_shard::<ReplaceStringFirstShard>();
  register_shard::<ReplaceStringAllShard>();
  register_shard::<InsertAtLineShard>();
}
