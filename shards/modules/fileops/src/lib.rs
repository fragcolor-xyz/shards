/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2025 Fragcolor Pte. Ltd. */

#[macro_use]
extern crate shards;

#[macro_use]
extern crate lazy_static;

use std::path::{Path, PathBuf};
use std::env;
use std::fs;
use std::collections::hash_map::DefaultHasher;
use std::hash::{Hash, Hasher};
use std::io::Read;

use shards::core::register_shard;
use shards::shard::Shard;
use shards::types::{
  common_type, AutoSeqVar, AutoTableVar, ClonedVar, ParamVar,
  STRING_TYPES, SEQ_OF_ANY_TABLE_TYPES, STRINGS_TYPES, FRAG_CC,
};
use shards::types::{Context, ExposedTypes, InstanceData, Type, Types, Var};
use shards::fourCharacterCode;

use bm25::{Language, LanguageMode, SearchEngine, SearchEngineBuilder};

// ============================================================================
// BM25 Index Object Type
// ============================================================================

/// BM25 search index wrapping a search engine and the original documents
pub struct BM25Index {
  engine: SearchEngine<u32>,
  documents: Vec<String>,
}

ref_counted_object_type_impl!(BM25Index);

lazy_static! {
  pub static ref BM25_INDEX_TYPE: Type = Type::object(FRAG_CC, fourCharacterCode(*b"bm25"));
  pub static ref BM25_INDEX_TYPE_VEC: Vec<Type> = vec![*BM25_INDEX_TYPE];
  pub static ref BM25_INDEX_VAR_TYPE: Type = Type::context_variable(&BM25_INDEX_TYPE_VEC);
}

use grep_regex::RegexMatcherBuilder;
use grep_searcher::{SearcherBuilder, Searcher, Sink, SinkMatch, SinkContext};

// ============================================================================
// Common Utilities
// ============================================================================

/// Maximum number of backup files to keep per original file
/// Older backups are automatically deleted when this limit is exceeded
const MAX_BACKUPS_PER_FILE: usize = 10;

/// Resolve a path relative to shell CWD, with tilde expansion
/// Returns an absolute, canonicalized path to prevent directory traversal
fn resolve_path(path: &str, shell_cwd: &Path) -> PathBuf {
  let resolved = if path.starts_with("~/") {
    // Expand home directory (cross-platform)
    if let Some(home) = dirs::home_dir() {
      home.join(&path[2..])
    } else {
      PathBuf::from(path)
    }
  } else if Path::new(path).is_absolute() {
    PathBuf::from(path)
  } else {
    // Relative to shell CWD
    shell_cwd.join(path)
  };

  // Canonicalize to resolve .. and . components and make absolute
  // This prevents path traversal attacks with relative paths
  match resolved.canonicalize() {
    Ok(canonical) => canonical,
    Err(_) => {
      // File doesn't exist yet - canonicalize parent directory at minimum
      // to ensure no .. components can escape the intended directory
      if let Some(parent) = resolved.parent() {
        if let Ok(canonical_parent) = parent.canonicalize() {
          if let Some(filename) = resolved.file_name() {
            return canonical_parent.join(filename);
          }
        }
      }
      // Fallback: return resolved path (parent canonicalization failed)
      resolved
    }
  }
}

/// Create a backup of the file if it exists
/// Automatically rotates backups, keeping only MAX_BACKUPS_PER_FILE most recent backups
fn create_backup(abs_path: &Path) -> Result<(), String> {
  let backup_dir = Path::new(".edits_backup");
  fs::create_dir_all(backup_dir).map_err(|e| format!("Failed to create backup directory: {}", e))?;

  // Use hash of absolute path + timestamp for unique backup filename
  let hash = calculate_hash(&abs_path.to_string_lossy());
  let timestamp = std::time::SystemTime::now()
    .duration_since(std::time::UNIX_EPOCH)
    .unwrap_or_default()
    .as_secs();
  let backup_path = backup_dir.join(format!("{}_{}.bak", hash, timestamp));

  // Attempt to copy, but ignore NotFound errors (new files don't need backup)
  match fs::copy(abs_path, &backup_path) {
    Ok(bytes) => {
      // Cleanup old backups for this file
      cleanup_old_backups(backup_dir, hash)?;
      Ok(())
    },
    Err(e) if e.kind() == std::io::ErrorKind::NotFound => Ok(()), // New file, no backup needed
    Err(e) => {
      shlog_error!("Failed to create backup: {}", e);
      Err(format!("Failed to create backup: {}", e))
    },
  }
}

/// Remove old backups for a file, keeping only the MAX_BACKUPS_PER_FILE most recent
fn cleanup_old_backups(backup_dir: &Path, file_hash: u64) -> Result<(), String> {
  let pattern = format!("{}_", file_hash);

  // Collect all backup files for this hash with their metadata
  let mut backups: Vec<(PathBuf, std::time::SystemTime)> = Vec::new();

  if let Ok(entries) = fs::read_dir(backup_dir) {
    for entry in entries.flatten() {
      if let Ok(filename) = entry.file_name().into_string() {
        if filename.starts_with(&pattern) && filename.ends_with(".bak") {
          if let Ok(metadata) = entry.metadata() {
            if let Ok(modified) = metadata.modified() {
              backups.push((entry.path(), modified));
            }
          }
        }
      }
    }
  }

  // If we have more than MAX_BACKUPS_PER_FILE, remove oldest ones
  if backups.len() > MAX_BACKUPS_PER_FILE {
    // Sort by modification time (oldest first)
    backups.sort_by_key(|(_, time)| *time);

    // Remove oldest backups
    let to_remove = backups.len() - MAX_BACKUPS_PER_FILE;
    for (path, _) in backups.iter().take(to_remove) {
      if let Err(e) = fs::remove_file(path) {
        shlog_error!("Warning: Failed to remove old backup {}: {}", path.display(), e);
      }
    }
  }

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
/// Returns a status message indicating success or failure
fn git_commit_file(path: &Path, operation: &str) -> Result<String, String> {
  use std::process::Command;

  let dir = path.parent().unwrap_or(path);

  // Use absolute path to avoid issues with special characters in filenames
  let abs_path = path.canonicalize()
    .unwrap_or_else(|_| path.to_path_buf());
  let abs_path_str = abs_path.to_string_lossy();

  let filename = path.file_name()
    .and_then(|n| n.to_str())
    .unwrap_or("unknown");

  // git add with -- separator for safety
  let add_result = Command::new("git")
    .args(&["add", "--", abs_path_str.as_ref()])
    .current_dir(dir)
    .output()
    .map_err(|e| format!("Git add failed: {}", e))?;

  if !add_result.status.success() {
    let stderr = String::from_utf8_lossy(&add_result.stderr);
    let msg = format!("Git add failed: {}", stderr.trim());
    shlog_error!("{}", msg);
    return Err(msg);
  }

  // git commit
  let commit_msg = format!("{} file {}", operation, filename);
  let commit_result = Command::new("git")
    .args(&["commit", "-m", &commit_msg])
    .current_dir(dir)
    .output()
    .map_err(|e| format!("Git commit failed: {}", e))?;

  if !commit_result.status.success() {
    let stderr = String::from_utf8_lossy(&commit_result.stderr);
    // Check if it's "nothing to commit" (not an error)
    if stderr.contains("nothing to commit") || stderr.contains("no changes added") {
      return Ok(" (no git changes)".to_string());
    }
    let msg = format!("Git commit failed: {}", stderr.trim());
    shlog_error!("{}", msg);
    return Err(msg);
  }

  Ok(" (committed to git)".to_string())
}

/// Check if path is a symlink and return error if so
/// We refuse to edit symlinks to avoid confusion about which file is being modified
fn check_not_symlink(path: &Path) -> Result<(), String> {
  if path.is_symlink() {
    return Err(format!("Refusing to edit symlink: {}. Please edit the target file directly.", path.display()));
  }
  Ok(())
}

/// Atomically write content to a file using temp-file-and-rename pattern
/// This prevents file corruption if the process crashes during write
fn atomic_write(path: &Path, content: &str) -> Result<(), String> {
  use std::io::Write;

  // Create temp file in same directory as target (required for atomic rename)
  let temp_path = if let Some(parent) = path.parent() {
    let filename = path.file_name()
      .and_then(|n| n.to_str())
      .unwrap_or("file");
    parent.join(format!(".{}.tmp.{}", filename, std::process::id()))
  } else {
    return Err("Cannot determine parent directory for atomic write".to_string());
  };

  // Write to temp file
  let mut temp_file = fs::File::create(&temp_path)
    .map_err(|e| format!("Failed to create temp file: {}", e))?;

  temp_file.write_all(content.as_bytes())
    .map_err(|e| format!("Failed to write to temp file: {}", e))?;

  // Sync to disk before rename
  temp_file.sync_all()
    .map_err(|e| format!("Failed to sync temp file: {}", e))?;

  // Close file before rename
  drop(temp_file);

  // Atomic rename
  fs::rename(&temp_path, path)
    .map_err(|e| {
      // Clean up temp file on failure
      let _ = fs::remove_file(&temp_path);
      format!("Failed to rename temp file: {}", e)
    })?;

  Ok(())
}

/// Check if file extension suggests it's a text file (fast path)
fn is_known_text_extension(path: &Path) -> bool {
  if let Some(ext) = path.extension() {
    if let Some(ext_str) = ext.to_str() {
      let ext_lower = ext_str.to_lowercase();
      matches!(ext_lower.as_str(),
        "txt" | "md" | "rs" | "toml" | "json" | "yaml" | "yml" |
        "xml" | "html" | "css" | "js" | "ts" | "py" | "c" | "cpp" |
        "h" | "hpp" | "sh" | "bash" | "zsh" | "fish" | "shs" |
        "log" | "csv" | "ini" | "cfg" | "conf" | "config"
      )
    } else {
      false
    }
  } else {
    false
  }
}

/// Check if file is binary using multiple heuristics
/// Uses fast path for known text extensions before content-based detection
fn is_binary_file(path: &Path) -> Result<bool, String> {
  // Fast path: check file extension first
  if is_known_text_extension(path) {
    return Ok(false);
  }

  // Slow path: content-based detection
  let mut buffer = [0u8; 32000]; // Increased buffer size for better detection
  let mut file = fs::File::open(path).map_err(|e| format!("Failed to open file: {}", e))?;

  let n = file.read(&mut buffer).map_err(|e| format!("Failed to read file: {}", e))?;
  if n == 0 {
    return Ok(false); // Empty file is treated as text
  }

  let sample = &buffer[..n];

  // Primary check: null bytes are a strong indicator of binary data
  if sample.contains(&0) {
    return Ok(true);
  }

  // Secondary check: validate UTF-8 encoding
  // If the file is not valid UTF-8, it's likely binary
  match std::str::from_utf8(sample) {
    Ok(_) => Ok(false), // Valid UTF-8, treat as text
    Err(_) => Ok(true), // Invalid UTF-8, treat as binary
  }
}

// ============================================================================
// Grep Shard
// ============================================================================

#[derive(shards::shard)]
#[shard_info("Grep", "Search for regex pattern in file(s) using ripgrep functionality")]
struct GrepShard {
  #[shard_required]
  required: ExposedTypes,

  #[shard_param("File", "File path to search in", [common_type::none, common_type::string, common_type::string_var])]
  file: ParamVar,

  #[shard_param("String", "Pure text string to search in (alternative to File)", [common_type::none, common_type::string, common_type::string_var])]
  string: ParamVar,

  #[shard_param("WorkDir", "Working directory for resolving relative paths", [common_type::none, common_type::string, common_type::string_var])]
  work_dir: ParamVar,

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
      string: ParamVar::default(),
      work_dir: ParamVar::default(),
      case_insensitive: ParamVar::new(false.into()),
      line_numbers: ParamVar::new(false.into()),
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

    // Validate that either File or String is provided (but not both)
    let has_file = !self.file.get().as_ref().is_none();
    let has_string = !self.string.get().as_ref().is_none();

    if !has_file && !has_string {
      return Err("Either File or String parameter must be provided");
    }

    if has_file && has_string {
      return Err("Cannot specify both File and String parameters - use only one");
    }

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
            match_table.0.insert_fast_static("line-number", &(lnum as i64).into());
          }
        }

        match_table.0.insert_fast_static("line", &Var::ephemeral_string(&line));

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
            context_table.0.insert_fast_static("line-number", &(lnum as i64).into());
          }
        }

        context_table.0.insert_fast_static("line", &Var::ephemeral_string(&line));

        self.output.0.push(&context_table.0.0);

        Ok(true) // Continue searching
      }
    }

    // Determine if using File or String mode and perform search
    let use_file = !self.file.get().as_ref().is_none();

    let max = if max_matches == 0 { i64::MAX } else { max_matches };

    let result = if use_file {
      // File mode
      let file_path_str: &str = self.file.get().as_ref().try_into().map_err(|_| {
        shlog_error!("File parameter must be a string");
        "File parameter must be a string"
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

      // Resolve file path
      let file_path = resolve_path(file_path_str, &work_dir);

      let mut sink = GrepSinkImpl {
        output: AutoSeqVar::new(),
        line_numbers,
        match_count: 0,
        max_matches: max,
      };

      let result = searcher.search_path(&matcher, &file_path, &mut sink);
      self.output = sink.output;
      result
    } else {
      // String mode
      let string_content: &str = self.string.get().as_ref().try_into().map_err(|_| {
        shlog_error!("String parameter must be a string");
        "String parameter must be a string"
      })?;

      let mut sink = GrepSinkImpl {
        output: AutoSeqVar::new(),
        line_numbers,
        match_count: 0,
        max_matches: max,
      };

      let result = searcher.search_slice(&matcher, string_content.as_bytes(), &mut sink);
      self.output = sink.output;
      result
    };

    // Handle search errors
    if let Err(e) = result {
      shlog_error!("Grep search failed: {}", e);
      return Err("Grep search failed");
    }

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

    // Check for symlinks
    check_not_symlink(&abs_path).map_err(|e| {
      shlog_error!("{}", e);
      "Cannot edit symlink"
    })?;

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

    // Write file atomically
    atomic_write(&abs_path, content).map_err(|e| {
      shlog_error!("Failed to write file: {}", e);
      "Failed to write file"
    })?;

    // Git commit if enabled and in repo
    let enable_git: bool = self.enable_git_commit.get().as_ref().try_into()
      .map_err(|_| "EnableGitCommit must be a boolean")?;

    let git_status = if enable_git && is_git_repo(&abs_path) {
      git_commit_file(&abs_path, "Creating").unwrap_or_else(|e| format!(" (git error: {})", e))
    } else {
      String::new()
    };

    let size = content.len();
    let result = format!("File created: {} ({} bytes){}", abs_path.display(), size, git_status);
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

    // Get parameters and validate immediately to fail fast
    let old_str: &str = self.old_str.get().as_ref().try_into().map_err(|_| "OldStr must be a string")?;
    let new_str: &str = self.new_str.get().as_ref().try_into().map_err(|_| "NewStr must be a string")?;

    // Validate empty string early before any I/O operations
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

    // Check for symlinks
    check_not_symlink(&abs_path).map_err(|e| {
      shlog_error!("{}", e);
      "Cannot edit symlink"
    })?;

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
    atomic_write(&abs_path, &new_content).map_err(|e| {
      shlog_error!("Failed to write file: {}", e);
      "Failed to write file"
    })?;

    // Git commit if enabled
    let enable_git: bool = self.enable_git_commit.get().as_ref().try_into()
      .map_err(|_| "EnableGitCommit must be a boolean")?;

    let git_status = if enable_git && is_git_repo(&abs_path) {
      git_commit_file(&abs_path, "Editing").unwrap_or_else(|e| format!(" (git error: {})", e))
    } else {
      String::new()
    };

    let result = format!("Replaced 1 occurrence in {}{}", abs_path.display(), git_status);
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

    // Get parameters and validate immediately to fail fast
    let old_str: &str = self.old_str.get().as_ref().try_into().map_err(|_| "OldStr must be a string")?;
    let new_str: &str = self.new_str.get().as_ref().try_into().map_err(|_| "NewStr must be a string")?;

    // Validate empty string early before any I/O operations
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

    // Check for symlinks
    check_not_symlink(&abs_path).map_err(|e| {
      shlog_error!("{}", e);
      "Cannot edit symlink"
    })?;

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
    atomic_write(&abs_path, &new_content).map_err(|e| {
      shlog_error!("Failed to write file: {}", e);
      "Failed to write file"
    })?;

    // Git commit if enabled
    let enable_git: bool = self.enable_git_commit.get().as_ref().try_into()
      .map_err(|_| "EnableGitCommit must be a boolean")?;

    let git_status = if enable_git && is_git_repo(&abs_path) {
      git_commit_file(&abs_path, "Editing").unwrap_or_else(|e| format!(" (git error: {})", e))
    } else {
      String::new()
    };

    let matches_text = if total_count == 1 { "match" } else { "matches" };
    let result = format!(
      "Replaced first occurrence in {} (found {} total {}){}",
      abs_path.display(),
      total_count,
      matches_text,
      git_status
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

    // Get parameters and validate immediately to fail fast
    let old_str: &str = self.old_str.get().as_ref().try_into().map_err(|_| "OldStr must be a string")?;
    let new_str: &str = self.new_str.get().as_ref().try_into().map_err(|_| "NewStr must be a string")?;

    // Validate empty string early before any I/O operations
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

    // Check for symlinks
    check_not_symlink(&abs_path).map_err(|e| {
      shlog_error!("{}", e);
      "Cannot edit symlink"
    })?;

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
    atomic_write(&abs_path, &new_content).map_err(|e| {
      shlog_error!("Failed to write file: {}", e);
      "Failed to write file"
    })?;

    // Git commit if enabled
    let enable_git: bool = self.enable_git_commit.get().as_ref().try_into()
      .map_err(|_| "EnableGitCommit must be a boolean")?;

    let git_status = if enable_git && is_git_repo(&abs_path) {
      git_commit_file(&abs_path, "Editing").unwrap_or_else(|e| format!(" (git error: {})", e))
    } else {
      String::new()
    };

    let occurrences_text = if count == 1 { "occurrence" } else { "occurrences" };
    let result = format!(
      "Replaced {} {} in {}{}",
      count,
      occurrences_text,
      abs_path.display(),
      git_status
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

  #[shard_param("LineNumber", "Line number to insert after (0 = very beginning before line 1, N = after line N). Use file's line count to append at end.", [common_type::int, common_type::int_var])]
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

    // Check for symlinks
    check_not_symlink(&abs_path).map_err(|e| {
      shlog_error!("{}", e);
      "Cannot edit symlink"
    })?;

    if !abs_path.exists() {
      shlog_error!("File not found: {}", abs_path.display());
      return Err("File not found");
    }

    let file_content = fs::read_to_string(&abs_path).map_err(|e| {
      shlog_error!("Failed to read file: {}", e);
      "Failed to read file"
    })?;

    // Check if original file had trailing newline to preserve it
    let had_trailing_newline = file_content.ends_with('\n');

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

    // Insert content using Vec::insert semantics
    // line_number = 0: insert at index 0 (very beginning, before line 1)
    // line_number = N: insert at index N (after line N, before line N+1)
    // This matches the JSON spec: "insert after line N" behavior
    lines.insert(line_num, content.to_string());

    // Write back atomically, preserving original file's trailing newline behavior
    let new_content = if had_trailing_newline {
      lines.join("\n") + "\n"
    } else {
      lines.join("\n")
    };
    atomic_write(&abs_path, &new_content).map_err(|e| {
      shlog_error!("Failed to write file: {}", e);
      "Failed to write file"
    })?;

    // Git commit if enabled
    let enable_git: bool = self.enable_git_commit.get().as_ref().try_into()
      .map_err(|_| "EnableGitCommit must be a boolean")?;

    let git_status = if enable_git && is_git_repo(&abs_path) {
      git_commit_file(&abs_path, "Inserting into").unwrap_or_else(|e| format!(" (git error: {})", e))
    } else {
      String::new()
    };

    let result = format!(
      "Inserted content at line {} in {}{}",
      line_num,
      abs_path.display(),
      git_status
    );
    self.output = Var::ephemeral_string(&result).into();
    Ok(Some(self.output.0))
  }
}

// ============================================================================
// BM25.Index Shard
// ============================================================================

#[derive(shards::shard)]
#[shard_info("BM25.Index", "Creates a BM25 search index from a sequence of strings")]
struct BM25IndexShard {
  #[shard_required]
  required: ExposedTypes,

  #[shard_param("Language", "Language for tokenization (e.g. \"english\", \"german\"). If none, auto-detects per document.", [common_type::none, common_type::string])]
  language: ClonedVar,

  output: ClonedVar,
}

impl Default for BM25IndexShard {
  fn default() -> Self {
    Self {
      required: ExposedTypes::new(),
      language: ClonedVar::default(),
      output: ClonedVar::default(),
    }
  }
}

#[shards::shard_impl]
impl Shard for BM25IndexShard {
  fn input_types(&mut self) -> &Types {
    &STRINGS_TYPES
  }

  fn output_types(&mut self) -> &Types {
    &BM25_INDEX_TYPE_VEC
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
    // Extract strings from input sequence, deduplicating
    let seq = input.as_seq().map_err(|e| {
      shlog_error!("Input must be a sequence of strings: {:?}", e);
      "Input must be a sequence of strings"
    })?;

    let mut seen = std::collections::HashSet::new();
    let mut documents: Vec<String> = Vec::with_capacity(seq.len());
    for item in seq.iter() {
      let s: &str = item.as_ref().try_into().map_err(|e| {
        shlog_error!("All items must be strings: {:?}", e);
        "All items must be strings"
      })?;
      let doc = s.to_string();
      if seen.insert(doc.clone()) {
        documents.push(doc);
      }
    }

    if documents.is_empty() {
      shlog_error!("Cannot create index from empty corpus");
      return Err("Cannot create index from empty corpus");
    }

    // Determine language mode
    let language_mode = if self.language.0.is_none() {
      LanguageMode::Detect
    } else {
      let lang_str: &str = self.language.0.as_ref().try_into()
        .map_err(|e| {
          shlog_error!("Language must be a string: {:?}", e);
          "Language must be a string"
        })?;
      let language = match lang_str.to_lowercase().as_str() {
        "arabic" => Language::Arabic,
        "danish" => Language::Danish,
        "dutch" => Language::Dutch,
        "english" => Language::English,
        "french" => Language::French,
        "german" => Language::German,
        "greek" => Language::Greek,
        "hungarian" => Language::Hungarian,
        "italian" => Language::Italian,
        "norwegian" => Language::Norwegian,
        "portuguese" => Language::Portuguese,
        "romanian" => Language::Romanian,
        "russian" => Language::Russian,
        "spanish" => Language::Spanish,
        "swedish" => Language::Swedish,
        "tamil" => Language::Tamil,
        "turkish" => Language::Turkish,
        _ => {
          shlog_error!("Unsupported language: {}", lang_str);
          return Err("Unsupported language. Supported: arabic, danish, dutch, english, french, german, greek, hungarian, italian, norwegian, portuguese, romanian, russian, spanish, swedish, tamil, turkish");
        }
      };
      LanguageMode::Fixed(language)
    };

    // Build search engine with corpus
    // Note: clone is necessary because bm25 stores its own copy internally via with_corpus,
    // but we also need the documents vec for retrieving content by ID in BM25.Query
    let engine = SearchEngineBuilder::<u32>::with_corpus(language_mode, documents.clone())
      .build();

    let index = BM25Index { engine, documents };

    self.output = Var::new_ref_counted(index, &*BM25_INDEX_TYPE).into();
    Ok(Some(self.output.0))
  }
}

// ============================================================================
// BM25.Query Shard
// ============================================================================

#[derive(shards::shard)]
#[shard_info("BM25.Query", "Queries a BM25 index and returns top matching documents with scores")]
struct BM25QueryShard {
  #[shard_required]
  required: ExposedTypes,

  #[shard_param("Index", "The BM25 index to query", [*BM25_INDEX_VAR_TYPE])]
  index: ParamVar,

  #[shard_param("TopK", "Maximum number of results to return", [common_type::int])]
  top_k: ClonedVar,

  output: AutoSeqVar,
}

impl Default for BM25QueryShard {
  fn default() -> Self {
    Self {
      required: ExposedTypes::new(),
      index: ParamVar::default(),
      top_k: 10i64.into(),
      output: AutoSeqVar::new(),
    }
  }
}

#[shards::shard_impl]
impl Shard for BM25QueryShard {
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
    let query: &str = input.try_into().map_err(|e| {
      shlog_error!("Query must be a string: {:?}", e);
      "Query must be a string"
    })?;

    let top_k: i64 = self.top_k.0.as_ref().try_into()
      .map_err(|e| {
        shlog_error!("TopK must be an integer: {:?}", e);
        "TopK must be an integer"
      })?;

    if top_k <= 0 {
      shlog_error!("TopK must be positive, got: {}", top_k);
      return Err("TopK must be positive");
    }

    // Get the index from parameter
    let index = unsafe {
      &*Var::from_ref_counted_object::<BM25Index>(&self.index.get(), &*BM25_INDEX_TYPE)
        .map_err(|e| {
          shlog_error!("Failed to get BM25 index: {}", e);
          e
        })?
    };

    // Perform search
    let results = index.engine.search(query, top_k as usize);

    // Clear previous results
    self.output.0.clear();

    // Build result sequence with content and score
    // Document IDs from bm25 correspond to the order documents were indexed
    for result in results {
      let mut result_table = AutoTableVar::new();

      let doc_id = result.document.id as usize;
      if let Some(content) = index.documents.get(doc_id) {
        result_table.0.insert_fast_static("content", &Var::ephemeral_string(content));
      } else {
        // This shouldn't happen if bm25 is working correctly, but handle defensively
        shlog_error!("Document ID {} out of bounds (corpus size: {})", doc_id, index.documents.len());
        result_table.0.insert_fast_static("content", &Var::ephemeral_string(""));
      }

      result_table.0.insert_fast_static("score", &(result.score as f64).into());

      self.output.0.push(&result_table.0.0);
    }

    Ok(Some(self.output.0.0))
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
  register_shard::<BM25IndexShard>();
  register_shard::<BM25QueryShard>();
}
