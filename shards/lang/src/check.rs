/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2026 Fragcolor Pte. Ltd. */

//! `shards check` — compose-only verification with structured, machine-readable
//! diagnostics for agent repair loops.
//!
//! The whole point of this command is to be the compiler-as-a-tool: it parses and
//! composes a script but NEVER schedules, warms up, or runs it (no side effects),
//! then reports every problem it can in a stable JSON schema. Three phases are
//! surfaced distinctly:
//!
//!   - `parse`     — syntax errors from the reader
//!   - `construct` — errors while building the wire graph (e.g. unknown shards)
//!   - `compose`   — whole-program type validation (the structured oracle)
//!
//! On top of the raw diagnostics it adds two repair aids:
//!   - did-you-mean suggestions for unknown shard names (edit distance over the
//!     registry), and
//!   - type-directed `candidates`: for an input/output type mismatch, the bridge
//!     shards whose input accepts the actual type and whose output produces an
//!     expected type.

use crate::eval::eval;
use crate::read::read;
use crate::Program;
use shards::core::{getShards, Core};
use shards::types::{AutoShardRef, ComposeDiagnostic, Mesh};
use std::collections::HashMap;
use std::path::Path;
use std::sync::atomic::AtomicBool;
use std::sync::Arc;

// Must mirror SHDiagnosticKind in include/shards/shards.h.
const KIND_GENERIC: u8 = 0;
const KIND_INPUT_TYPE_MISMATCH: u8 = 1;
const KIND_COMPOSE_ERROR: u8 = 2;
const KIND_UNKNOWN_SHARD: u8 = 3;

// SHType values we special-case during candidate matching (see SHType in shards.h).
const SHTYPE_NONE: i32 = 0;
const SHTYPE_ANY: i32 = 1;
const SHTYPE_SEQ: i32 = 56;
const SHTYPE_TABLE: i32 = 57;

/// A "scalar" for candidate purposes: anything that isn't a container or wildcard.
/// A scalar/value mismatch is almost never repaired by producing a Seq/Table, so we
/// use this to keep container targets from drowning out real value converters.
fn is_scalar(b: i32) -> bool {
  b != SHTYPE_ANY && b != SHTYPE_NONE && b != SHTYPE_SEQ && b != SHTYPE_TABLE
}

/// A single rendered type in the JSON output.
#[derive(serde::Serialize, Clone)]
struct JsonType {
  /// Canonical type string (e.g. `String`, `[Int]`, `{none: Any}`).
  name: String,
  /// SHType value, -1 if unknown. Stable across releases; useful for tooling.
  basic_type: i32,
}

/// One diagnostic in the JSON report.
#[derive(serde::Serialize)]
struct JsonDiagnostic {
  /// `parse` | `construct` | `compose`.
  phase: &'static str,
  /// `error` | `warning`.
  severity: &'static str,
  /// Stable classification: `syntax` | `unknown-shard` | `input-type-mismatch` |
  /// `compose-error` | `generic`.
  kind: &'static str,
  message: String,
  file: String,
  line: u32,
  column: u32,
  #[serde(skip_serializing_if = "Option::is_none")]
  shard: Option<String>,
  #[serde(skip_serializing_if = "Option::is_none")]
  actual: Option<JsonType>,
  #[serde(skip_serializing_if = "Vec::is_empty")]
  expected: Vec<JsonType>,
  #[serde(skip_serializing_if = "Option::is_none")]
  param_index: Option<i32>,
  /// Edit-distance suggestions for misspelled shard names.
  #[serde(skip_serializing_if = "Vec::is_empty")]
  did_you_mean: Vec<String>,
  /// Type-directed bridge shards that could fix an input-type mismatch.
  #[serde(skip_serializing_if = "Vec::is_empty")]
  candidates: Vec<String>,
}

/// The top-level JSON document emitted by `shards check --json`.
#[derive(serde::Serialize)]
struct CheckReport {
  ok: bool,
  file: String,
  diagnostics: Vec<JsonDiagnostic>,
}

/// Resolve a source-file id (as tracked through parse/compose) to a path string.
fn source_file_name(file_id: u32) -> Option<String> {
  unsafe {
    let swl = (*Core).getSourceFileName.unwrap_unchecked()(file_id);
    if swl.string.is_null() || swl.len == 0 {
      None
    } else {
      Some(swl.str().to_string())
    }
  }
}

/// Case-insensitive Levenshtein distance (small inputs; allocates two rows).
fn levenshtein(a: &str, b: &str) -> usize {
  let a: Vec<char> = a.chars().flat_map(|c| c.to_lowercase()).collect();
  let b: Vec<char> = b.chars().flat_map(|c| c.to_lowercase()).collect();
  if a.is_empty() {
    return b.len();
  }
  if b.is_empty() {
    return a.len();
  }
  let mut prev: Vec<usize> = (0..=b.len()).collect();
  let mut curr: Vec<usize> = vec![0; b.len() + 1];
  for (i, &ca) in a.iter().enumerate() {
    curr[0] = i + 1;
    for (j, &cb) in b.iter().enumerate() {
      let cost = if ca == cb { 0 } else { 1 };
      curr[j + 1] = (prev[j + 1] + 1).min(curr[j] + 1).min(prev[j] + cost);
    }
    std::mem::swap(&mut prev, &mut curr);
  }
  prev[b.len()]
}

/// Suggest up to `max` registered shard names close to `name`.
fn did_you_mean(name: &str, all: &[String], max: usize) -> Vec<String> {
  // Threshold scales with the typo'd name length; short names need exact-ish hits.
  let threshold = (name.chars().count() / 3).max(2);
  let mut scored: Vec<(usize, &String)> = all
    .iter()
    .map(|s| (levenshtein(name, s), s))
    .filter(|(d, _)| *d <= threshold)
    .collect();
  scored.sort_by(|(da, sa), (db, sb)| da.cmp(db).then_with(|| sa.cmp(sb)));
  scored.into_iter().take(max).map(|(_, s)| s.clone()).collect()
}

/// A compact view of a registered shard's input/output basic types, used to find
/// bridge shards for the candidates engine.
struct ShardTypes {
  name: String,
  inputs: Vec<i32>,
  outputs: Vec<i32>,
}

/// Build the registry index of input/output basic types. This creates and destroys
/// every registered shard once, so it is built lazily — only when at least one
/// input-type-mismatch diagnostic actually needs candidates.
fn build_shard_index() -> Vec<ShardTypes> {
  let mut index = Vec::new();
  for cname in getShards() {
    let name = cname.to_string_lossy().to_string();
    if let Some(shard) = AutoShardRef::create(&name, None) {
      let inputs: Vec<i32> = shard.0.input_types().iter().map(|t| t.basicType as i32).collect();
      let outputs: Vec<i32> = shard.0.output_types().iter().map(|t| t.basicType as i32).collect();
      index.push(ShardTypes { name, inputs, outputs });
    }
  }
  index
}

/// Find bridge shards that could repair an input-type mismatch: a shard whose input
/// accepts `actual` and whose output produces one of `expected`.
///
/// Two refinements keep the list signal-dense:
///   - Target outputs drop the `Any`/`None` wildcards; for a *scalar* actual we also
///     drop container (`Seq`/`Table`) targets, which otherwise match every
///     seq-producing shard (a scalar mismatch is not fixed by producing a container).
///   - Results are ranked: converters whose input is *exactly* `actual` (e.g.
///     `ParseInt: String -> Int`) come before ones that merely accept `Any`, so the
///     genuinely useful bridges survive the truncation to `max`.
fn find_candidates(
  index: &[ShardTypes],
  actual: i32,
  expected: &[i32],
  offending: Option<&str>,
  max: usize,
) -> Vec<String> {
  if actual < 0 || actual == SHTYPE_NONE {
    return Vec::new();
  }
  let non_generic: Vec<i32> = expected
    .iter()
    .copied()
    .filter(|&b| b != SHTYPE_ANY && b != SHTYPE_NONE)
    .collect();
  if non_generic.is_empty() {
    return Vec::new();
  }
  // Prefer scalar targets when the mismatch is a scalar; fall back to the full set
  // only if there are no scalar targets (i.e. a genuine container is expected).
  let targets: Vec<i32> = if is_scalar(actual) {
    let scalars: Vec<i32> = non_generic.iter().copied().filter(|&b| is_scalar(b)).collect();
    if scalars.is_empty() {
      non_generic
    } else {
      scalars
    }
  } else {
    non_generic
  };

  let mut scored: Vec<(u8, String)> = index
    .iter()
    .filter(|s| Some(s.name.as_str()) != offending)
    .filter_map(|s| {
      let in_exact = s.inputs.iter().any(|&b| b == actual);
      let in_any = s.inputs.iter().any(|&b| b == SHTYPE_ANY);
      let out_ok = s.outputs.iter().any(|&b| b != SHTYPE_ANY && targets.contains(&b));
      if out_ok && (in_exact || in_any) {
        Some((if in_exact { 0u8 } else { 1u8 }, s.name.clone()))
      } else {
        None
      }
    })
    .collect();
  scored.sort_by(|(ta, na), (tb, nb)| ta.cmp(tb).then_with(|| na.cmp(nb)));
  scored.truncate(max);
  scored.into_iter().map(|(_, n)| n).collect()
}

/// Map a structured compose diagnostic onto its JSON form, attaching candidates
/// for input-type mismatches (building the registry index lazily on first need).
fn compose_diag_to_json(
  d: &ComposeDiagnostic,
  fallback_file: &str,
  index: &mut Option<Vec<ShardTypes>>,
) -> JsonDiagnostic {
  let kind = match d.kind {
    KIND_INPUT_TYPE_MISMATCH => "input-type-mismatch",
    KIND_COMPOSE_ERROR => "compose-error",
    KIND_UNKNOWN_SHARD => "unknown-shard",
    KIND_GENERIC => "generic",
    _ => "generic",
  };

  let actual = d.actual.as_ref().map(|t| JsonType {
    name: t.name.clone(),
    basic_type: t.basic_type,
  });
  let expected: Vec<JsonType> = d
    .expected
    .iter()
    .map(|t| JsonType {
      name: t.name.clone(),
      basic_type: t.basic_type,
    })
    .collect();

  let mut candidates = Vec::new();
  if d.kind == KIND_INPUT_TYPE_MISMATCH {
    if let Some(actual_t) = d.actual.as_ref() {
      let expected_basics: Vec<i32> = d.expected.iter().map(|t| t.basic_type).collect();
      let idx = index.get_or_insert_with(build_shard_index);
      let offending = if d.shard_name.is_empty() {
        None
      } else {
        Some(d.shard_name.as_str())
      };
      candidates = find_candidates(idx, actual_t.basic_type, &expected_basics, offending, 10);
    }
  }

  let file = if d.file.is_empty() {
    fallback_file.to_string()
  } else {
    d.file.clone()
  };

  JsonDiagnostic {
    phase: "compose",
    severity: "error",
    kind,
    message: d.message.clone(),
    file,
    line: d.line,
    column: d.column,
    shard: if d.shard_name.is_empty() {
      None
    } else {
      Some(d.shard_name.clone())
    },
    actual,
    expected,
    param_index: if d.param_index >= 0 {
      Some(d.param_index)
    } else {
      None
    },
    did_you_mean: Vec::new(),
    candidates,
  }
}

/// Render the report in a concise, human-readable (clang-like) form to stdout.
fn print_human(report: &CheckReport) {
  if report.ok {
    println!("{}: ok", report.file);
    return;
  }
  for d in &report.diagnostics {
    println!(
      "{}:{}:{}: {} [{}/{}]: {}",
      d.file, d.line, d.column, d.severity, d.phase, d.kind, d.message
    );
    if let Some(actual) = &d.actual {
      if !d.expected.is_empty() {
        let exp: Vec<&str> = d.expected.iter().map(|t| t.name.as_str()).collect();
        println!("    found: {} | expected: {}", actual.name, exp.join(" | "));
      }
    }
    if !d.candidates.is_empty() {
      println!("    candidate bridges: {}", d.candidates.join(", "));
    }
    if !d.did_you_mean.is_empty() {
      println!("    did you mean: {}", d.did_you_mean.join(", "));
    }
  }
  let n = report.diagnostics.len();
  println!("{} diagnostic{}", n, if n == 1 { "" } else { "s" });
}

/// Entry point for the `check` subcommand. Returns the process exit code:
///   0 = ok, 1 = problems found, 2 = could not run the check (e.g. file missing).
///
/// `defines` mirrors `run`'s command-line `key:value` defines: they are injected
/// before construct/compose so a script that references command-line defines (or
/// whose composition branches on them) is checked in the same configuration it
/// would run in.
pub fn check_command(
  file: &str,
  include: Vec<String>,
  defines: HashMap<String, String>,
  json: bool,
  cancellation_token: Arc<AtomicBool>,
) -> i32 {
  // Resolve the file and set the root path so includes resolve exactly as `run` would.
  let file_path = match dunce::canonicalize(Path::new(file)) {
    Ok(p) => p,
    Err(_) => {
      eprintln!("shards check: input file '{}' not found", file);
      return 2;
    }
  };
  let mut file_content = match std::fs::read_to_string(&file_path) {
    Ok(c) => c,
    Err(e) => {
      eprintln!("shards check: failed to read '{}': {}", file, e);
      return 2;
    }
  };
  // The reader expects a trailing newline.
  file_content.push('\n');

  let parent_path = file_path.parent().and_then(|p| p.to_str()).unwrap_or(".").to_string();

  // Canonicalize the `-I` include dirs BEFORE setRootPath: `run` resolves them
  // against the original working directory, but setRootPath calls `fs::current_path`
  // and changes the CWD to the script's parent. Canonicalizing a relative `-I` (e.g.
  // `-I.`) after that would resolve it against the script's dir instead, so a relative
  // include dir would silently point at the wrong place. Do it first to match `run`.
  //
  // A `-I` that does not resolve is a hard error (exit 2), exactly as `run` fails on
  // it — NOT a silent skip. Silently dropping a bad include dir is the worst outcome
  // for a diagnostic tool: a mistyped/missing `-I` then looks like "this include dir
  // wasn't honored", and the real failure surfaces later as a confusing
  // "include not found". Fail loudly on stderr so the bad invocation is obvious while
  // stdout stays clean for `--json`.
  let mut include_paths = Vec::new();
  for path in &include {
    match dunce::canonicalize(std::path::PathBuf::from(path)) {
      Ok(p) => include_paths.push(p.to_string_lossy().to_string()),
      Err(e) => {
        eprintln!("shards check: include dir '{}' not found: {}", path, e);
        return 2;
      }
    }
  }

  if let Ok(c_parent) = std::ffi::CString::new(parent_path.clone()) {
    unsafe { (*Core).setRootPath.unwrap_unchecked()(c_parent.as_ptr()) };
  }

  let display_file = file_path.to_string_lossy().to_string();
  let mut diagnostics: Vec<JsonDiagnostic> = Vec::new();

  // Phase 1: parse.
  let ast: Program = match read(
    &file_content,
    file_path.to_str().unwrap(),
    parent_path.clone(),
    include_paths,
  ) {
    Ok(ast) => ast,
    Err(e) => {
      let file = source_file_name(e.loc.file).unwrap_or_else(|| display_file.clone());
      diagnostics.push(JsonDiagnostic {
        phase: "parse",
        severity: "error",
        kind: "syntax",
        message: e.message,
        file,
        line: e.loc.line,
        column: e.loc.column,
        shard: None,
        actual: None,
        expected: Vec::new(),
        param_index: None,
        did_you_mean: Vec::new(),
        candidates: Vec::new(),
      });
      return finish_report(display_file, diagnostics, json);
    }
  };

  // Phase 2: construct (build the wire graph). Unknown shards surface here.
  let wire = match eval(&ast, "root", defines, cancellation_token) {
    Ok(w) => w,
    Err(e) => {
      let file = source_file_name(e.loc.file).unwrap_or_else(|| display_file.clone());
      // "Shard X does not exist" -> attach did-you-mean for X.
      let (kind, shard, suggestions) = match parse_unknown_shard(&e.message) {
        Some(name) => {
          let names: Vec<String> = getShards()
            .iter()
            .map(|c| c.to_string_lossy().to_string())
            .collect();
          ("unknown-shard", Some(name.clone()), did_you_mean(&name, &names, 5))
        }
        None => ("generic", None, Vec::new()),
      };
      diagnostics.push(JsonDiagnostic {
        phase: "construct",
        severity: "error",
        kind,
        message: e.message,
        file,
        line: e.loc.line,
        column: e.loc.column,
        shard,
        actual: None,
        expected: Vec::new(),
        param_index: None,
        did_you_mean: suggestions,
        candidates: Vec::new(),
      });
      return finish_report(display_file, diagnostics, json);
    }
  };

  // Phase 3: compose (whole-program type validation). Never schedules or runs.
  let mesh = Mesh::default();
  let result = mesh.compose_check(wire.0);

  let mut index: Option<Vec<ShardTypes>> = None;
  for d in &result.diagnostics {
    diagnostics.push(compose_diag_to_json(d, &display_file, &mut index));
  }
  // If compose failed but produced no structured diagnostics, surface the raw error.
  if result.failed && result.diagnostics.is_empty() {
    diagnostics.push(JsonDiagnostic {
      phase: "compose",
      severity: "error",
      kind: "generic",
      message: if result.error.is_empty() {
        "Composition failed".to_string()
      } else {
        result.error.clone()
      },
      file: display_file.clone(),
      line: 0,
      column: 0,
      shard: None,
      actual: None,
      expected: Vec::new(),
      param_index: None,
      did_you_mean: Vec::new(),
      candidates: Vec::new(),
    });
  }

  finish_report(display_file, diagnostics, json)
}

/// Extract the shard name out of the "Shard X does not exist" construct error.
fn parse_unknown_shard(message: &str) -> Option<String> {
  let rest = message.strip_prefix("Shard ")?;
  let name = rest.strip_suffix(" does not exist")?;
  if name.is_empty() {
    None
  } else {
    Some(name.to_string())
  }
}

#[cfg(test)]
mod tests {
  use super::*;

  fn st(name: &str, inputs: &[i32], outputs: &[i32]) -> ShardTypes {
    ShardTypes {
      name: name.to_string(),
      inputs: inputs.to_vec(),
      outputs: outputs.to_vec(),
    }
  }

  #[test]
  fn levenshtein_basics() {
    assert_eq!(levenshtein("Log", "Log"), 0);
    assert_eq!(levenshtein("Logg", "Log"), 1);
    assert_eq!(levenshtein("", "abc"), 3);
    assert_eq!(levenshtein("abc", ""), 3);
    assert_eq!(levenshtein("kitten", "sitting"), 3);
    // case-insensitive
    assert_eq!(levenshtein("LOG", "log"), 0);
  }

  #[test]
  fn did_you_mean_finds_close_typo() {
    let all = vec![
      "Log".to_string(),
      "Math.Add".to_string(),
      "ParseInt".to_string(),
    ];
    let s = did_you_mean("Logg", &all, 5);
    assert!(s.contains(&"Log".to_string()), "got {:?}", s);
  }

  #[test]
  fn did_you_mean_empty_for_far_off() {
    let all = vec!["Log".to_string(), "Math.Add".to_string()];
    assert!(did_you_mean("CompletelyUnrelatedName", &all, 5).is_empty());
  }

  #[test]
  fn is_scalar_classifies() {
    assert!(is_scalar(52)); // String
    assert!(is_scalar(4)); // Int
    assert!(!is_scalar(SHTYPE_ANY));
    assert!(!is_scalar(SHTYPE_NONE));
    assert!(!is_scalar(SHTYPE_SEQ));
    assert!(!is_scalar(SHTYPE_TABLE));
  }

  #[test]
  fn parse_unknown_shard_extracts_name() {
    assert_eq!(
      parse_unknown_shard("Shard Logg does not exist"),
      Some("Logg".to_string())
    );
    assert_eq!(
      parse_unknown_shard("Shard fbl/set-tracked does not exist"),
      Some("fbl/set-tracked".to_string())
    );
    assert_eq!(parse_unknown_shard("some other error"), None);
    // The unknown-function error has a different shape and must not match.
    assert_eq!(
      parse_unknown_shard("unknown built-in function or definition: fbl/x"),
      None
    );
  }

  #[test]
  fn candidates_prefer_converters_and_drop_container_noise() {
    // actual = String(52); expected = Int(4) and a Seq([Any], 56).
    let index = vec![
      st("ParseInt", &[52], &[4]),          // String -> Int : exact-input (tier 0)
      st("ToInt", &[SHTYPE_ANY], &[4]),     // Any -> Int : tier 1
      st("CSV.Read", &[52], &[SHTYPE_SEQ]), // String -> Seq : dropped (scalar actual)
      st("Count", &[SHTYPE_ANY], &[4]),     // Any -> Int : tier 1
      st("Math.Add", &[4], &[4]),           // offending : excluded
    ];
    let c = find_candidates(&index, 52, &[4, SHTYPE_SEQ], Some("Math.Add"), 10);
    assert!(c.contains(&"ParseInt".to_string()), "got {:?}", c);
    assert!(
      !c.contains(&"CSV.Read".to_string()),
      "seq-output must be dropped for a scalar mismatch: {:?}",
      c
    );
    assert!(!c.contains(&"Math.Add".to_string()), "offending must be excluded");
    // exact-input converter ranks before Any-input ones.
    let pi = c.iter().position(|x| x == "ParseInt").unwrap();
    if let Some(ti) = c.iter().position(|x| x == "ToInt") {
      assert!(pi < ti, "exact-input should rank before Any-input: {:?}", c);
    }
  }

  #[test]
  fn candidates_empty_on_degenerate_input() {
    let index = vec![st("ParseInt", &[52], &[4])];
    assert!(find_candidates(&index, 52, &[], None, 10).is_empty()); // no expected
    assert!(find_candidates(&index, SHTYPE_NONE, &[4], None, 10).is_empty()); // actual None
    assert!(find_candidates(&index, 52, &[SHTYPE_ANY], None, 10).is_empty()); // only wildcard expected
  }

  #[test]
  fn candidates_respect_max() {
    let index: Vec<ShardTypes> = (0..20).map(|i| st(&format!("S{:02}", i), &[52], &[4])).collect();
    assert_eq!(find_candidates(&index, 52, &[4], None, 5).len(), 5);
  }
}

/// Emit the report (JSON or human) and compute the exit code.
fn finish_report(file: String, diagnostics: Vec<JsonDiagnostic>, json: bool) -> i32 {
  let ok = !diagnostics.iter().any(|d| d.severity == "error");
  let report = CheckReport { ok, file, diagnostics };
  if json {
    match serde_json::to_string_pretty(&report) {
      Ok(s) => println!("{}", s),
      Err(e) => {
        eprintln!("shards check: failed to serialize report: {}", e);
        return 2;
      }
    }
  } else {
    print_human(&report);
  }
  if ok {
    0
  } else {
    1
  }
}
