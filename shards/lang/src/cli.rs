use crate::error::Error;
use crate::read::{get_dependencies, read_with_env, ReadEnv};
use crate::{eval, formatter, Program};
use crate::{eval::eval, eval::new_cancellation_token, read::read};
use clap::{arg, CommandFactory, Parser};
use clap_complete::{generate, Shell};
use shards::core::Core;
use shards::types::{get_enum_info, type_to_string, AutoShardRef, EnumInfoId, Mesh};
use shards::util::from_raw_parts_allow_null;
use shards::{
  fourCharacterCode, shlog, shlog_debug, shlog_error, SHCore, SHOptionalString, SHTypeInfo,
  SHType_ContextVar as SHTYPE_CONTEXT_VAR, SHType_Enum as SHTYPE_ENUM, SHType_Seq as SHTYPE_SEQ,
  SHType_Table as SHTYPE_TABLE, GIT_VERSION, SHARDS_CURRENT_ABI,
};
use std::collections::HashMap;
use std::ffi::CStr;
use std::fs;
use std::io::Write;
use std::os::raw::c_char;
use std::path::Path;
use std::sync::atomic::AtomicBool;
use std::sync::{atomic, Arc};

// One discovery row, owned by the core; freed via shards_discovery_free.
#[repr(C)]
struct ShShardIndexEntry {
  name: *const c_char,
  input: *const c_char,
  output: *const c_char,
  summary: *const c_char,
  score: f64,
}

extern "C" {
  fn shardsInterface(version: u32) -> *mut SHCore;
  fn shards_install_signal_handlers();
  fn shards_decompress_strings();
  // Discovery index + ranked search, served live from the core registry so the CLI
  // (`enumerate`/`search`) and the Shards.Index / Shards.Search shards share one impl.
  fn shards_discovery_index(out_count: *mut u64) -> *mut ShShardIndexEntry;
  fn shards_discovery_search(
    query: *const c_char,
    limit: i64,
    out_count: *mut u64,
  ) -> *mut ShShardIndexEntry;
  fn shards_discovery_free(entries: *mut ShShardIndexEntry, count: u64);
}

// `shards pak` (single-file packaging) lives in the `pak` module. The startup
// self-check below asks it whether *this* executable carries an embedded script.
use crate::pak;

#[derive(Debug, clap::Args)]
struct RunArgs {
  /// The script file to execute
  #[arg(value_hint = clap::ValueHint::FilePath)]
  file: String,

  /// Decompress help strings before running the script
  #[arg(long, short = 'd', default_value = "false", action)]
  decompress_strings: bool,

  /// Skip changing the current working directory to the script's directory
  #[arg(long, short = 'c', action)]
  skip_cwd: bool,

  /// Additional include directories for script imports
  #[arg(long, short = 'I')]
  include: Vec<String>,

  /// Arguments to pass to the script (format: key:value)
  #[arg(num_args = 0..)]
  args: Vec<String>,
}

#[derive(Debug, clap::Subcommand)]
enum Commands {
  /// Format a Shards source file
  Format {
    /// The file to format (use '-' for stdin)
    #[arg(value_hint = clap::ValueHint::FilePath)]
    file: String,
    /// Format the file in-place (default outputs to stdout)
    #[arg(long, short = 'i', action)]
    inline: bool,
    /// Write formatted output to a specific file
    #[arg(long, short = 'o')]
    output: Option<String>,
  },
  /// Run formatter tests
  Test {},
  /// Create and run a new Shards script
  New(RunArgs),
  /// Run a Shards script
  Run(RunArgs),
  /// Type-check a Shards script: parse + compose only, never runs it.
  ///
  /// Emits structured, machine-readable diagnostics (with `--json`) suitable for
  /// CI and agent repair loops. Exit code: 0 = ok, 1 = problems found, 2 = the
  /// check could not run (e.g. missing file).
  Check {
    /// The script source file to check
    #[arg(value_hint = clap::ValueHint::FilePath)]
    file: String,
    /// Emit machine-readable JSON diagnostics
    #[arg(long, short = 'j', action)]
    json: bool,
    /// Additional include directories for imports
    #[arg(long, short = 'I')]
    include: Vec<String>,
    /// Defines to inject before composing (format: key:value), exactly as `run`.
    /// Required when the script references command-line defines, or to check a
    /// specific configuration when composition branches on a define.
    #[arg(num_args = 0..)]
    args: Vec<String>,
  },
  /// Evaluate Shards code from stdin
  Eval {
    /// Decompress help strings before evaluation
    #[arg(long, short = 'd', default_value = "false", action)]
    decompress_strings: bool,
    /// Arguments to pass to the script (format: key:value)
    #[arg(num_args = 0..)]
    args: Vec<String>,
  },
  /// Build a binary AST from a Shards source file
  Build {
    /// The script source file to compile
    #[arg(value_hint = clap::ValueHint::FilePath)]
    file: String,
    /// Output file path for the compiled binary
    #[arg(long, short = 'o', default_value = "out.sho")]
    output: String,
    /// Output as JSON AST instead of binary
    #[arg(long, short = 'j', action)]
    json: bool,
    /// Additional include directories for imports
    #[arg(long, short = 'I')]
    include: Vec<String>,
    /// Generate dependency file in Makefile format
    #[arg(long, short = 'd')]
    depfile: Option<String>,
  },
  /// Pack a Shards script into a standalone, single-file executable
  ///
  /// Compiles the script and appends it to a copy of this `shards` binary,
  /// producing a self-contained executable that runs the script directly with
  /// no interpreter, runtime files or toolchain required.
  Pak {
    /// The script source file to pack
    #[arg(value_hint = clap::ValueHint::FilePath)]
    file: String,
    /// Output executable path (defaults to the script name without extension)
    #[arg(long, short = 'o')]
    output: Option<String>,
    /// Additional include directories for imports
    #[arg(long, short = 'I')]
    include: Vec<String>,
    /// Signing identity (macOS: codesign -s <id>, default ad-hoc; Windows: signtool /n <subject>)
    #[arg(long)]
    sign: Option<String>,
    /// Do not sign the produced binary (macOS: leaves an invalid signature; testing only)
    #[arg(long, action)]
    no_sign: bool,
    /// macOS: notarize the produced binary after signing (requires --notarize-profile)
    #[arg(long, action)]
    notarize: bool,
    /// macOS notarization keychain profile (xcrun notarytool --keychain-profile)
    #[arg(long)]
    notarize_profile: Option<String>,
  },
  /// Generate JSON AST from a Shards source file
  AST {
    /// The script source file to parse
    #[arg(value_hint = clap::ValueHint::FilePath)]
    file: String,
    /// Output file path for the JSON AST
    #[arg(long, short = 'o', default_value = "out.sho")]
    output: String,
    /// Additional include directories for imports
    #[arg(long, short = 'I')]
    include: Vec<String>,
  },
  /// Load and execute a compiled binary Shards file
  Load {
    /// The compiled binary file (.sho) to execute
    #[arg(value_hint = clap::ValueHint::FilePath)]
    file: String,
    /// Decompress help strings before execution
    #[arg(long, short = 'd', default_value = "false", action)]
    decompress_strings: bool,
    /// Arguments to pass to the script (format: key:value)
    #[arg(num_args = 0..)]
    args: Vec<String>,
  },
  /// Search and display Shards documentation
  Docs {
    /// Name of the shard or enum to look up
    #[arg()]
    name: String,
    /// Type of documentation to search: "shard" or "enum"
    #[arg(long = "type", short = 't', default_value = "shard", action)]
    type_: String,
    /// Emit the full signature as machine-readable JSON (for agents)
    #[arg(long, short = 'j', action)]
    json: bool,
  },
  /// List shards as a compact one-line index (name, in→out, summary)
  ///
  /// The always-loaded discovery layer for agents; drill into any entry with
  /// `shards docs <name> --json`.
  Enumerate {
    /// Only list shards whose name contains this substring (case-insensitive)
    #[arg(long, short = 'f')]
    filter: Option<String>,
    /// Emit as JSON
    #[arg(long, short = 'j', action)]
    json: bool,
  },
  /// Keyword search over the shard index (matches name and summary)
  Search {
    /// The query (case-insensitive substring)
    #[arg()]
    query: String,
    /// Emit as JSON
    #[arg(long, short = 'j', action)]
    json: bool,
  },
  /// Generate shell completion scripts
  Completions {
    /// The shell to generate completions for
    #[arg(value_enum)]
    shell: Shell,
  },
}

#[derive(Debug, clap::Parser)]
#[command(name = "shards")]
#[command(version = env!("CARGO_PKG_VERSION"))]
#[command(about = "Shards programming language - command line tools and script executor")]
#[command(author = "Fragcolor Team")]
#[command(long_about = "Shards is a flow-based programming language with a unique data flow paradigm.\nUse this tool to run scripts, build binaries, format code, and access documentation.")]
struct Cli {
  #[command(subcommand)]
  command: Commands,
}

#[derive(Debug, clap::Parser)]
#[command(name = "shards run")]
struct SimpleCLI {
  #[command(flatten)]
  run_args: RunArgs,
}

fn generate_completions(shell: Shell) {
  let mut cmd = Cli::command();
  let name = cmd.get_name().to_string();
  generate(shell, &mut cmd, name, &mut std::io::stdout());
}

pub fn process_args(argc: i32, argv: *const *const c_char, _no_cancellation: bool) -> i32 {
  let cancellation_token = new_cancellation_token();

  let args: Vec<String> = unsafe {
    from_raw_parts_allow_null(argv, argc as usize)
      .iter()
      .map(|&arg| {
        let c_str = CStr::from_ptr(arg);
        c_str.to_str().unwrap().to_owned()
      })
      .collect()
  };

  unsafe {
    shards::core::Core = shardsInterface(SHARDS_CURRENT_ABI as u32);
    (*shards::core::Core).init.unwrap_unchecked()();
    shards_install_signal_handlers();
    shlog_debug!("Shards git version: {}", GIT_VERSION);
  }

  // Self-extracting path: if this executable was produced by `shards pak`, it
  // carries an embedded compiled script. Detect and run it directly, ignoring
  // normal subcommand parsing. Extra argv is forwarded to the script as defines.
  match pak::load_self_payload() {
    Ok(Some(payload)) => {
      let res = deserialize_sho(&payload).and_then(|ast| {
        let script_args: Vec<String> = args.iter().skip(1).cloned().collect();
        execute_seq(&script_args, ast, cancellation_token).map_err(|e| -> Error { e.into() })
      });
      return finish(res);
    }
    Ok(None) => {} // not a packed binary, continue with normal CLI handling
    Err(e) => {
      shlog_error!("Failed to load embedded script: {}", e);
      return 1;
    }
  }

  let cli = Cli::try_parse_from(args.clone());
  let res = match cli {
    Ok(cli) => match &cli.command {
      Commands::Build {
        file,
        output,
        include,
        depfile,
        json,
      } => build(file, &output, include.to_vec(), depfile.as_deref(), *json),
      Commands::Pak {
        file,
        output,
        include,
        sign,
        no_sign,
        notarize,
        notarize_profile,
      } => pak(
        file,
        output.as_deref(),
        include.to_vec(),
        pak::SignOpts {
          no_sign: *no_sign,
          identity: sign.clone(),
          notarize: *notarize,
          notarize_profile: notarize_profile.clone(),
        },
      ),
      Commands::AST {
        file,
        output,
        include,
      } => build(file, &output, include.to_vec(), None, true),
      Commands::Load {
        file,
        decompress_strings,
        args,
      } => load(file, args, *decompress_strings, cancellation_token),
      Commands::New(args) => execute(args, cancellation_token),
      Commands::Run(args) => execute(args, cancellation_token),
      Commands::Check {
        file,
        json,
        include,
        args,
      } => {
        // `check` controls its own exit code (and keeps stdout clean for `--json`),
        // so it bypasses the generic error-logging `finish` path.
        return crate::check::check_command(
          file,
          include.to_vec(),
          parse_defines(args),
          *json,
          cancellation_token,
        );
      }
      Commands::Eval {
        decompress_strings,
        args,
      } => {
        if *decompress_strings {
          unsafe {
            shards_decompress_strings();
          }
        }
        match std::io::read_to_string(std::io::stdin()) {
          Ok(input) => {
            match read(&input, "<stdin>", ".".to_string(), vec![]) {
              Ok(ast) => {
                match execute_seq(args, ast, cancellation_token) {
                  Ok(_) => Ok(()),
                  Err(e) => Err(format!("Failed to execute stdin: {}", e).into()),
                }
              }
              Err(e) => {
                shlog!("Error: {:?}", e);
                Err("Failed to parse stdin".into())
              }
            }
          }
          Err(e) => Err(format!("Failed to read stdin: {}", e).into()),
        }
      }
      Commands::Format {
        file,
        output,
        inline,
      } => format(file, output, *inline),
      Commands::Test {} => formatter::run_tests(),
      Commands::Docs { name, type_, json } => help(name, type_, *json),
      Commands::Enumerate { filter, json } => enumerate(filter.as_deref(), *json),
      Commands::Search { query, json } => search(query, *json),
      Commands::Completions { shell } => {
        generate_completions(*shell);
        Ok(())
      }
    },
    // Fall back to the bare "shards <file> [args...]" form only when the first
    // argument actually looks like something to run (a path, a .shs, or a flag).
    // Otherwise surface clap's error — which says "unrecognized subcommand" and
    // suggests a close one — instead of the misleading "Input file <verb> not found"
    // you'd get from trying to run e.g. `shards doc` as a script.
    Err(orig_err) => {
      let first = args.get(1).map(|s| s.as_str()).unwrap_or("");
      let looks_like_run =
        first.starts_with('-') || first.ends_with(".shs") || Path::new(first).is_file();
      if looks_like_run {
        match SimpleCLI::try_parse_from(args) {
          Ok(cli) => execute(&cli.run_args, cancellation_token),
          Err(_e) => Err(Box::new(orig_err) as Box<dyn std::error::Error>),
        }
      } else {
        Err(Box::new(orig_err) as Box<dyn std::error::Error>)
      }
    }
  };

  finish(res)
}

/// Map a command result onto a process exit code, logging any error.
fn finish(res: Result<(), Error>) -> i32 {
  if let Err(e) = res {
    shlog_error!("Error: {}", e);
    1
  } else {
    0
  }
}

// Alternative simpler implementation to avoid recursion issues
// Create a wrapper that handles indentation without recursion
fn print_type_indented<W: Write>(w: &mut W, t: &SHTypeInfo, indent: &str) -> std::io::Result<()> {
  writeln!(
    w,
    "{}Type: `{}`",
    indent,
    type_to_string(t.basicType.into())
  )?;
  let next_indent = format!("{}    ", indent); // 4 spaces for consistent indentation
  let branch_indent = format!("{}  └─", indent); // No trailing space after box drawing character

  match t.basicType {
    SHTYPE_SEQ => {
      let types_seq = unsafe { t.details.seqTypes };
      for i in 0..types_seq.len {
        let t = unsafe { &*types_seq.elements.offset(i as isize) };
        writeln!(w, "{} Seq of:", branch_indent)?; // Add newline after "Seq of:"
        print_type_indented(w, t, &next_indent)?;
      }
    }
    SHTYPE_TABLE => {
      let types_table = unsafe { t.details.table };
      let table_types = types_table.types;
      for i in 0..table_types.len {
        let t = unsafe { &*table_types.elements.offset(i as isize) };
        writeln!(w, "{} Table of:", branch_indent)?; // Add newline after "Table of:"
        print_type_indented(w, t, &next_indent)?;
      }
      let table_keys = types_table.keys;
      for i in 0..table_keys.len {
        let t = unsafe { &*table_keys.elements.offset(i as isize) };
        writeln!(w, "{} Table key: `{}`", branch_indent, t)?;
      }
    }
    SHTYPE_CONTEXT_VAR => {
      let types_context_var = unsafe { t.details.contextVarTypes };
      for i in 0..types_context_var.len {
        let t = unsafe { &*types_context_var.elements.offset(i as isize) };
        writeln!(w, "{} Variable of:", branch_indent)?; // Add newline after "Variable of:"
        print_type_indented(w, t, &next_indent)?;
      }
    }
    SHTYPE_ENUM => {
      let enum_vendor = unsafe { t.details.enumeration.vendorId };
      let enum_type = unsafe { t.details.enumeration.typeId };
      let enum_info = get_enum_info(EnumInfoId::VendorTypePair(enum_vendor, enum_type));
      if let Some(enum_info) = enum_info {
        let name = unsafe { CStr::from_ptr(enum_info.name).to_str().unwrap() };
        writeln!(w, "{} Enum: `{}`", branch_indent, name)?;
      } else {
        writeln!(
          w,
          "{} Enum: (Vendor: {}, Type: {})",
          branch_indent, enum_vendor, enum_type
        )?;
      }
    }
    _ => {}
  }
  Ok(())
}

// Update print_type to use the non-recursive approach
pub fn print_type<W: Write>(w: &mut W, t: &SHTypeInfo) -> std::io::Result<()> {
  print_type_indented(w, t, "")
}

pub fn get_optional_string(os: SHOptionalString) -> &'static str {
  let c_str = if !os.string.is_null() {
    os.string
  } else {
    if os.crc != 0 {
      unsafe { (*Core).getCompressedString.unwrap_unchecked()(os.crc) }
    } else {
      panic!("SHOptionalString is empty");
    }
  };
  unsafe { CStr::from_ptr(c_str).to_str().unwrap() }
}

pub fn help_to_writer<W: Write>(w: &mut W, name: &str, type_: &str) -> Result<(), Error> {
  unsafe {
    shards_decompress_strings();
  }

  match type_ {
    "shard" => {
      let shard = AutoShardRef::create(name, None);
      if let Some(shard) = shard {
        let help_text = shard.0.help();
        let input_help = shard.0.input_help();
        let output_help = shard.0.output_help();
        let input_types = shard.0.input_types();
        let output_types = shard.0.output_types();
        let parameters = shard.0.parameters();

        // Title with box drawing characters
        writeln!(w, "Help for `{}`", name)?;

        // Description section
        if let Some(help) = help_text {
          if !help.is_empty() {
            writeln!(w, "Description:")?;
            writeln!(w, "   {}", help)?;
            writeln!(w)?;
          }
        }

        // Input section with types
        writeln!(w, "Input:")?;
        if let Some(help) = input_help {
          if !help.is_empty() {
            writeln!(w, "   {}", help)?;
          }
        }
        if !input_types.is_empty() {
          for input_type in input_types {
            print_type_indented(w, &input_type, "   ")?;
          }
        } else {
          writeln!(w, "   No specific input type requirements")?;
        }
        writeln!(w)?;

        // Output section with types
        writeln!(w, "Output:")?;
        if let Some(help) = output_help {
          if !help.is_empty() {
            writeln!(w, "   {}", help)?;
          }
        }
        if !output_types.is_empty() {
          for output_type in output_types {
            print_type_indented(w, &output_type, "   ")?;
          }
        } else {
          writeln!(w, "   No specific output type information")?;
        }
        writeln!(w)?;

        // Parameters section
        if !parameters.is_empty() {
          writeln!(w, "🔧 Parameters:")?;
          for parameter in parameters {
            let name = unsafe { CStr::from_ptr(parameter.name).to_str().unwrap() };
            writeln!(w, "   ● `{}`", name)?;

            let help = get_optional_string(parameter.help);
            if !help.is_empty() {
              writeln!(w, "     Description: {}", help)?;
            }

            let types = parameter.valueTypes;
            if types.len > 0 {
              writeln!(w, "     Accepted types:")?;
              for i in 0..types.len {
                let t = unsafe { &*types.elements.offset(i as isize) };
                print_type_indented(w, t, "     ")?;
              }
            }
            writeln!(w)?;
          }
        }

        Ok(())
      } else {
        Err(format!("Shard '{}' not found", name).into())
      }
    }
    "enum" => {
      let info = get_enum_info(EnumInfoId::String(name));
      if let Some(info) = info {
        // Title
        writeln!(w, "Help for enum `{}`", name)?;

        // Description section
        let help = get_optional_string(info.help);
        if !help.is_empty() {
          writeln!(w, "Description:")?;
          writeln!(w, "   {}", help)?;
          writeln!(w)?;
        }

        // Values section
        writeln!(w, "Values:")?;
        assert!(info.values.len == info.labels.len);
        for i in 0..info.values.len {
          let label = unsafe {
            let label_ptr = *info.labels.elements.offset(i as isize);
            if label_ptr.is_null() {
              "<null>"
            } else {
              CStr::from_ptr(label_ptr).to_str().unwrap()
            }
          };
          let value = unsafe { &*info.values.elements.offset(i as isize) };
          let description =
            get_optional_string(unsafe { *info.descriptions.elements.offset(i as isize) });

          writeln!(w, "   ● `{}` = {}", label, value)?;
          if !description.is_empty() {
            writeln!(w, "     Description: {}", description)?;
          }
        }

        Ok(())
      } else {
        Err(format!("Enum '{}' not found", name).into())
      }
    }
    _ => Err("Invalid help type. Supported types are 'shard' and 'enum'".into()),
  }
}

fn help(name: &str, type_: &str, json: bool) -> Result<(), Error> {
  if json {
    return help_json(name, type_);
  }
  let mut stdout = std::io::stdout();
  help_to_writer(&mut stdout, name, type_)
}

/// Render a type (recursively) as structured JSON. `type_to_string` only covers the
/// top-level basic type, so containers (`Seq`/`Table`/`ContextVar`) and enums recurse
/// to preserve element/key/variant detail an agent needs to write correct code.
fn type_to_json(t: &SHTypeInfo) -> serde_json::Value {
  use serde_json::json;
  let base = type_to_string(t.basicType.into());
  match t.basicType {
    SHTYPE_SEQ => {
      let seq = unsafe { t.details.seqTypes };
      let elements: Vec<_> = (0..seq.len)
        .map(|i| type_to_json(unsafe { &*seq.elements.offset(i as isize) }))
        .collect();
      json!({ "type": base, "basic_type": t.basicType as i32, "elements": elements })
    }
    SHTYPE_TABLE => {
      let table = unsafe { t.details.table };
      let values: Vec<_> = (0..table.types.len)
        .map(|i| type_to_json(unsafe { &*table.types.elements.offset(i as isize) }))
        .collect();
      let keys: Vec<String> = (0..table.keys.len)
        .map(|i| format!("{}", unsafe { &*table.keys.elements.offset(i as isize) }))
        .collect();
      json!({ "type": base, "basic_type": t.basicType as i32, "values": values, "keys": keys })
    }
    SHTYPE_CONTEXT_VAR => {
      let cv = unsafe { t.details.contextVarTypes };
      let of: Vec<_> = (0..cv.len)
        .map(|i| type_to_json(unsafe { &*cv.elements.offset(i as isize) }))
        .collect();
      json!({ "type": base, "basic_type": t.basicType as i32, "of": of })
    }
    SHTYPE_ENUM => {
      let vendor = unsafe { t.details.enumeration.vendorId };
      let typ = unsafe { t.details.enumeration.typeId };
      let name = get_enum_info(EnumInfoId::VendorTypePair(vendor, typ))
        .map(|info| unsafe { CStr::from_ptr(info.name).to_str().unwrap().to_string() });
      json!({ "type": base, "basic_type": t.basicType as i32, "enum": name })
    }
    _ => json!({ "type": base, "basic_type": t.basicType as i32 }),
  }
}

/// Machine-readable counterpart to `help_to_writer` — the drill-down tool an agent
/// uses after locating a shard via `enumerate`/`search`. Served live from the binary,
/// so it can never drift from the runtime.
fn help_json(name: &str, type_: &str) -> Result<(), Error> {
  use serde_json::json;
  unsafe {
    shards_decompress_strings();
  }

  let doc = match type_ {
    "shard" => {
      let shard =
        AutoShardRef::create(name, None).ok_or_else(|| format!("Shard '{}' not found", name))?;
      let s = &shard.0;

      let input_types: Vec<_> = s.input_types().iter().map(type_to_json).collect();
      let output_types: Vec<_> = s.output_types().iter().map(type_to_json).collect();

      let mut parameters = Vec::new();
      for (i, p) in s.parameters().iter().enumerate() {
        let pname = unsafe { CStr::from_ptr(p.name).to_str().unwrap_or("") };
        let phelp = get_optional_string(p.help);
        let ptypes: Vec<_> = (0..p.valueTypes.len)
          .map(|j| type_to_json(unsafe { &*p.valueTypes.elements.offset(j as isize) }))
          .collect();
        let default = format!("{}", s.get_parameter(i as i32));
        parameters.push(json!({
          "name": pname,
          "help": phelp,
          "types": ptypes,
          "default": default,
        }));
      }

      json!({
        "name": name,
        "kind": "shard",
        "help": s.help().unwrap_or(""),
        "input": { "help": s.input_help().unwrap_or(""), "types": input_types },
        "output": { "help": s.output_help().unwrap_or(""), "types": output_types },
        "parameters": parameters,
      })
    }
    "enum" => {
      let info =
        get_enum_info(EnumInfoId::String(name)).ok_or_else(|| format!("Enum '{}' not found", name))?;
      assert!(info.values.len == info.labels.len);
      let mut values = Vec::new();
      for i in 0..info.values.len {
        let label = unsafe {
          let p = *info.labels.elements.offset(i as isize);
          if p.is_null() {
            ""
          } else {
            CStr::from_ptr(p).to_str().unwrap_or("")
          }
        };
        let value = unsafe { *info.values.elements.offset(i as isize) } as i64;
        let description =
          get_optional_string(unsafe { *info.descriptions.elements.offset(i as isize) });
        values.push(json!({ "label": label, "value": value, "description": description }));
      }
      json!({
        "name": name,
        "kind": "enum",
        "help": get_optional_string(info.help),
        "values": values,
      })
    }
    _ => return Err("Invalid help type. Supported types are 'shard' and 'enum'".into()),
  };

  println!("{}", serde_json::to_string_pretty(&doc)?);
  Ok(())
}

/// One line of the Tier-1 shard index.
struct ShardSummary {
  name: String,
  input: String,
  output: String,
  summary: String,
}

/// Read the heap-owned entries returned by a `shards_discovery_*` C entry into owned
/// Rust values (paired with relevance score), then release the C allocation. The index
/// build + ranking live in the core (discovery::buildIndex / discovery::search), so the
/// CLI can never drift from the Shards.Index / Shards.Search shards.
fn collect_index(ptr: *mut ShShardIndexEntry, count: u64) -> Vec<(ShardSummary, f64)> {
  let mut out = Vec::new();
  if ptr.is_null() {
    return out;
  }
  let cstr = |p: *const c_char| -> String {
    if p.is_null() {
      String::new()
    } else {
      unsafe { CStr::from_ptr(p) }.to_string_lossy().into_owned()
    }
  };
  unsafe {
    for i in 0..count {
      let e = &*ptr.add(i as usize);
      out.push((
        ShardSummary {
          name: cstr(e.name),
          input: cstr(e.input),
          output: cstr(e.output),
          summary: cstr(e.summary),
        },
        e.score,
      ));
    }
    shards_discovery_free(ptr, count);
  }
  out
}

fn print_index(items: &[ShardSummary], json: bool) -> Result<(), Error> {
  use serde_json::json;
  if json {
    let arr: Vec<_> = items
      .iter()
      .map(|i| {
        json!({ "name": i.name, "input": i.input, "output": i.output, "summary": i.summary })
      })
      .collect();
    println!("{}", serde_json::to_string_pretty(&arr)?);
  } else {
    for i in items {
      if i.summary.is_empty() {
        println!("{:<32} {} → {}", i.name, i.input, i.output);
      } else {
        println!("{:<32} {} → {}  {}", i.name, i.input, i.output, i.summary);
      }
    }
  }
  Ok(())
}

fn enumerate(filter: Option<&str>, json: bool) -> Result<(), Error> {
  let mut count: u64 = 0;
  let ptr = unsafe { shards_discovery_index(&mut count) };
  let mut items: Vec<ShardSummary> = collect_index(ptr, count)
    .into_iter()
    .map(|(s, _)| s)
    .collect();
  if let Some(f) = filter {
    let f = f.to_lowercase();
    items.retain(|i| i.name.to_lowercase().contains(&f));
  }
  print_index(&items, json)
}

fn search(query: &str, json: bool) -> Result<(), Error> {
  use serde_json::json;
  let cq = std::ffi::CString::new(query)?;
  let mut count: u64 = 0;
  let ptr = unsafe { shards_discovery_search(cq.as_ptr(), 0, &mut count) };
  let scored = collect_index(ptr, count);
  if json {
    let arr: Vec<_> = scored
      .iter()
      .map(|(i, score)| {
        json!({ "name": i.name, "input": i.input, "output": i.output, "summary": i.summary, "score": score })
      })
      .collect();
    println!("{}", serde_json::to_string_pretty(&arr)?);
  } else {
    for (i, score) in &scored {
      if i.summary.is_empty() {
        println!("{:<32} {} → {}  [{:.2}]", i.name, i.input, i.output, score);
      } else {
        println!(
          "{:<32} {} → {}  [{:.2}]  {}",
          i.name, i.input, i.output, score, i.summary
        );
      }
    }
  }
  Ok(())
}

fn format(file: &str, output: &Option<String>, inline: bool) -> Result<(), Error> {
  if output.is_some() && inline {
    return Err("Cannot use both -i and -o".into());
  }

  let mut in_str = if file == "-" {
    std::io::read_to_string(std::io::stdin()).unwrap()
  } else {
    fs::read_to_string(file)?
  };

  let newline_style = formatter::detect_newline_style(&in_str);

  // add new line at the end of the file to be able to parse it correctly
  newline_style.push_to_str(&mut in_str);

  if inline {
    let mut buf = std::io::BufWriter::new(Vec::new());
    let mut v = formatter::FormatterVisitor::new(&mut buf, &in_str);
    v.newline_style = newline_style;

    crate::rule_visitor::process(&in_str, &mut v)?;

    fs::write(file, &buf.into_inner()?[..])?;
  } else {
    let mut out_stream: Box<dyn std::io::Write> = if let Some(out) = output {
      Box::new(fs::File::create(out)?)
    } else {
      Box::new(std::io::stdout())
    };

    let mut v = formatter::FormatterVisitor::new(out_stream.as_mut(), &in_str);
    v.newline_style = newline_style;
    crate::rule_visitor::process(&in_str, &mut v)?;
  }

  std::io::stdout().flush()?;

  Ok(())
}

fn load(
  file: &str,
  args: &Vec<String>,
  decompress_strings: bool,
  cancellation_token: Arc<AtomicBool>,
) -> Result<(), Error> {
  if decompress_strings {
    unsafe {
      shards_decompress_strings();
    }
  }

  shlog!("Loading file");
  shlog!("Parsing binary file: {}", file);

  let ast = {
    let file_content = std::fs::read(file).map_err(|_| "File not found")?;
    deserialize_sho(&file_content)?
  };

  Ok(execute_seq(&args, ast, cancellation_token)?)
}

/// Parse `key:value` CLI arguments into the defines map shared by `run`, `load`,
/// `eval` and `check`. The value may itself contain `:` (everything after the first
/// colon is kept), supports `\:` escaping, and is unquoted if wrapped in double quotes.
fn parse_defines(args: &[String]) -> HashMap<String, String> {
  let mut defines = HashMap::new();
  for arg in args {
    shlog_debug!("arg: {}", arg);
    // find the first colon and split it; the value is everything after (may contain ':')
    let mut split = arg.split(':');
    let key = split.next().unwrap();
    let value = split.collect::<Vec<&str>>().join(":");
    // unescape '\:' then drop surrounding quotes if present
    let value = value.replace("\\:", ":");
    let value = value.trim_matches('"');
    defines.insert(key.to_owned(), value.to_owned());
  }
  defines
}

fn execute_seq(
  args: &Vec<String>,
  ast: Program,
  cancellation_token: Arc<AtomicBool>,
) -> Result<(), &'static str> {
  let defines = parse_defines(args);

  let wire = {
    eval(&ast, "root", defines, cancellation_token.clone()).map_err(|e| {
      shlog_error!("Error: {:?}", e);
      "Failed to evaluate file"
    })?
  };
  // enlarge stack
  wire.set_stack_size(eval::EVAL_STACK_SIZE);

  let mut mesh = Mesh::default();
  if let Err(e) = mesh.compose(wire.0) {
    shlog_error!("Failed to compose mesh: {}", e);
    return Err("Failed to compose mesh");
  }
  mesh.schedule(wire.0, false);

  loop {
    if cancellation_token.load(atomic::Ordering::Relaxed) {
      break;
    }

    if !mesh.tick() || mesh.is_empty() {
      break;
    }

    // still yield to other threads
    // consider that this will be basically ignored if there is a @run in the script anyway
    // this is merely a safety measure when no @run is present
    std::thread::yield_now();
  }

  let info = wire.get_info();
  if info.failed {
    let msg = std::str::from_utf8(unsafe {
      from_raw_parts_allow_null(
        info.failureMessage.string as *const u8,
        info.failureMessage.len as usize,
      )
    })
    .unwrap();
    Err("Failed to execute file")
  } else {
    Ok(())
  }
}

/// Parse a Shards source file into its AST, returning (dependency paths, ast).
fn read_program(file: &str, include: Vec<String>) -> Result<(Vec<String>, Program), Error> {
  let file_path = Path::new(&file);
  let file_path = dunce::canonicalize(file_path).map_err(|_| format!("Input file {} not found", file))?;
  let mut file_content = std::fs::read_to_string(&file_path).map_err(|_| "File not found")?;
  // add new line at the end of the file to be able to parse it correctly
  file_content.push('\n');

  // get absolute parent path of the file
  let parent_path = file_path.parent().unwrap().to_str().unwrap();

  let mut env = ReadEnv::new(file_path.to_str().unwrap(), parent_path.to_string(), include);
  let ast = read_with_env(&file_content, &mut env).map_err(|e| {
    shlog!("Error: {:?}", e);
    "Failed to parse file"
  })?;
  let mut deps = get_dependencies(&env)
    .iter()
    .map(|x| {
      dunce::canonicalize(x)
        .map_err(|_| "Failed to canonicalize path")
        .map(|x| x.to_string_lossy().to_string())
    })
    .collect::<Result<Vec<String>, _>>()?;
  // Add the main file as well
  let p = dunce::canonicalize(&file_path)
    .map_err(|_| "Failed to canonicalize path")?
    .to_string_lossy()
    .to_string();
  deps.push(p);
  Ok((deps, ast))
}

/// Serialize an AST into the binary `.sho` representation:
/// `"SHRD"` (big-endian FourCC) + ABI version (little-endian u32) + flexbuffers AST.
fn serialize_sho(ast: &Program) -> Vec<u8> {
  let encoded_bin = flexbuffers::to_vec(ast).unwrap();
  let mut out = Vec::with_capacity(8 + encoded_bin.len());
  out.extend_from_slice(fourCharacterCode(*b"SHRD").to_be_bytes().as_ref());
  out.extend_from_slice(SHARDS_CURRENT_ABI.to_le_bytes().as_ref());
  out.extend_from_slice(&encoded_bin);
  out
}

/// Deserialize a binary `.sho` payload, validating the `SHRD`+ABI header.
fn deserialize_sho(bytes: &[u8]) -> Result<Program, Error> {
  if bytes.len() < 8 {
    return Err("Invalid .sho payload: too small".into());
  }
  let magic = i32::from_be_bytes([bytes[0], bytes[1], bytes[2], bytes[3]]);
  if magic != fourCharacterCode(*b"SHRD") {
    return Err("Invalid .sho payload: bad magic".into());
  }
  let version = u32::from_le_bytes([bytes[4], bytes[5], bytes[6], bytes[7]]);
  if version != SHARDS_CURRENT_ABI {
    return Err(format!("Incompatible .sho ABI version {} (expected {})", version, SHARDS_CURRENT_ABI).into());
  }
  flexbuffers::from_slice(&bytes[8..]).map_err(|e| format!("Failed to decode .sho: {}", e).into())
}

/// Detect and load a script packed into this executable by `shards pak`.
/// Returns `Ok(None)` when this is a plain (un-packed) `shards` binary.
/// Pack a script into a standalone executable: compile it, then embed it into a
/// copy of this `shards` binary using the platform-native container mechanism
/// (see the `pak` module). The result is a signed, runnable single file.
fn pak(
  file: &str,
  output: Option<&str>,
  include: Vec<String>,
  sign: pak::SignOpts,
) -> Result<(), Error> {
  shlog!("Packing file: {}", file);

  // Compile the script to its binary .sho representation (in memory).
  let (_deps, ast) = read_program(file, include)?;
  let payload = serialize_sho(&ast);

  // Determine the output path: default to the script's stem (no extension),
  // adding `.exe` on Windows.
  let out_path = match output {
    Some(o) => o.to_string(),
    None => {
      let stem = Path::new(file)
        .file_stem()
        .and_then(|s| s.to_str())
        .unwrap_or("app")
        .to_string();
      if cfg!(windows) {
        format!("{}.exe", stem)
      } else {
        stem
      }
    }
  };

  pak::embed_payload(&payload, &out_path, &sign)?;

  shlog!(
    "Packed '{}' -> '{}' ({} bytes script embedded)",
    file,
    out_path,
    payload.len()
  );
  Ok(())
}

fn build(
  file: &str,
  output: &str,
  include: Vec<String>,
  depfile: Option<&str>,
  as_json: bool,
) -> Result<(), Error> {
  shlog!("Parsing file: {}", file);

  let (deps, ast) = read_program(file, include)?;

  // write sequence to file
  {
    let mut file = std::fs::File::create(output).unwrap();
    let mut writer = std::io::BufWriter::new(&mut file);

    if !as_json {
      writer.write_all(&serialize_sho(&ast)).unwrap();
    } else {
      let encoded_json = serde_json::to_string_pretty(&ast).unwrap();
      writer.write_all(encoded_json.as_bytes()).unwrap();
    }
  }

  if let Some(out_dep_file) = depfile {
    let mut file = std::fs::File::create(out_dep_file).unwrap();
    let mut writer = std::io::BufWriter::new(&mut file);

    writer.write_all(output.as_bytes()).unwrap();
    writer.write_all(b": ").unwrap();
    for dep in deps {
      writer.write_all(dep.as_bytes()).unwrap();
      writer.write_all(b" ").unwrap();
    }
  }
  Ok(())
}

fn execute(eargs: &RunArgs, cancellation_token: Arc<AtomicBool>) -> Result<(), Error> {
  let RunArgs {
    file,
    decompress_strings,
    skip_cwd,
    include: in_include_paths,
    args,
  } = eargs;

  if *decompress_strings {
    unsafe {
      shards_decompress_strings();
    }
  }

  shlog_debug!("Evaluating file: {}", file);

  let ast = {
    let file_path = Path::new(&file);
    let file_path = dunce::canonicalize(file_path).map_err(|_| format!("Input file {} not found", file))?;
    let mut file_content = std::fs::read_to_string(file).map_err(|_| "File not found")?;
    // add new line at the end of the file to be able to parse it correctly
    file_content.push('\n');

    let parent_path = file_path.parent().unwrap().to_str().unwrap();

    let mut include_paths = Vec::new();
    for path in in_include_paths {
      let path = std::path::PathBuf::from(path);
      let path = dunce::canonicalize(path.clone())
        .map_err(|x| format!("Failed to canonicalize path: {} ({:?})", x, path))?;
      include_paths.push(path.to_string_lossy().to_string());
    }

    if !*skip_cwd {
      // get absolute parent path of the file
      let c_parent_path = std::ffi::CString::new(parent_path).unwrap();
      // set it as root path
      unsafe { (*Core).setRootPath.unwrap()(c_parent_path.as_ptr() as *const c_char) };
    }

    read(
      &file_content,
      file_path.to_str().unwrap(),
      parent_path.to_string(),
      include_paths,
    )
    .map_err(|e| {
      shlog!("Error: {:?}", e);
      "Failed to parse file"
    })?
  };

  Ok(execute_seq(args, ast, cancellation_token)?)
}
