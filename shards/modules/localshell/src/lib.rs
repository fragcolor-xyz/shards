/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2025 Fragcolor Pte. Ltd. */

//! LocalShell module: persistent, interactive local shell sessions backed by a
//! PTY. All the session/buffer/prompt machinery lives in `shards-shell-common`;
//! this crate only provides the PTY transport and the `LocalShell.*` shards.

#![allow(non_upper_case_globals)]
#![allow(non_camel_case_types)]
#![allow(non_snake_case)]

#[macro_use]
extern crate shards;

#[macro_use]
extern crate lazy_static;

use shards::core::register_shard;
use shards::core::run_blocking;
use shards::core::BlockingShard;
use shards::fourCharacterCode;
use shards::shard::Shard;
use shards::types::common_type;
use shards::types::ClonedVar;
use shards::types::Context;
use shards::types::ExposedTypes;
use shards::types::InstanceData;
use shards::types::ParamVar;
use shards::types::Type;
use shards::types::Types;
use shards::types::Var;
use shards::types::BOOL_TYPES;
use shards::types::FRAG_CC;
use shards::types::NONE_TYPES;
use shards::types::STRING_TYPES;

use shards_shell_common as sc;

use portable_pty::{CommandBuilder, PtySize};
use std::io::{Read, Write};
use std::sync::atomic::{AtomicBool, AtomicU16, AtomicUsize, Ordering};
use std::sync::{Arc, Mutex};

// ============================================================================
// PTY transport
// ============================================================================

struct PtyTransport {
  pair: Arc<Mutex<Option<portable_pty::PtyPair>>>,
  reader: Arc<Mutex<Box<dyn Read + Send>>>,
  writer: Arc<Mutex<Box<dyn Write + Send>>>,
  child: Arc<Mutex<Option<Box<dyn portable_pty::Child + Send + Sync>>>>,
}

impl sc::ShellTransport for PtyTransport {
  fn read_into(&self, buf: &mut [u8]) -> sc::ReadOutcome {
    match self.reader.lock() {
      Ok(mut reader) => match reader.read(buf) {
        Ok(n) if n > 0 => sc::ReadOutcome::Data(n),
        // EOF or any read error: the process has gone away.
        Ok(_) => sc::ReadOutcome::Ended,
        Err(_) => sc::ReadOutcome::Ended,
      },
      Err(_) => sc::ReadOutcome::Ended,
    }
  }

  fn write_all(&self, data: &[u8]) -> Result<(), sc::TransportError> {
    let mut writer = self
      .writer
      .lock()
      .map_err(|_| sc::TransportError::Other("Writer lock poisoned"))?;
    writer
      .write_all(data)
      .map_err(|_| sc::TransportError::Other("Failed to write to shell (IO error)"))?;
    writer
      .flush()
      .map_err(|_| sc::TransportError::Other("Failed to flush writer (IO error)"))?;
    Ok(())
  }

  fn resize(&self, rows: u16, cols: u16) -> Result<(), &'static str> {
    let pair_guard = self.pair.lock().map_err(|_| "PTY pair lock poisoned")?;
    if let Some(ref pair) = *pair_guard {
      pair
        .master
        .resize(PtySize {
          rows,
          cols,
          pixel_width: 0,
          pixel_height: 0,
        })
        .map_err(|_| "Failed to resize PTY")?;
      Ok(())
    } else {
      Err("PTY pair has been dropped")
    }
  }

  fn close(&self) {
    // Kill the shell child process first.
    if let Ok(mut child_opt) = self.child.lock() {
      if let Some(mut child) = child_opt.take() {
        shlog_trace!("Killing shell child process");
        let _ = child.kill();
        let _ = child.wait();
      }
    }

    // CRITICAL: drop the PTY pair to close file descriptors. This causes the
    // blocking read() in the reader thread to return with EOF.
    if let Ok(mut pair_opt) = self.pair.lock() {
      if let Some(pair) = pair_opt.take() {
        shlog_trace!("Dropping PTY pair to close file descriptors");
        drop(pair);
        shlog_trace!("PTY pair dropped");
      }
    }
  }
}

// ============================================================================
// Session object wrapper (local newtype satisfies the orphan rule for the
// ref-counted object type impl).
// ============================================================================

mod local_shell {
  use super::*;

  pub struct LocalShellSession(pub sc::ShellSession);

  ref_counted_object_type_impl!(LocalShellSession);
}

use local_shell::*;

const LOCAL_SHELL_MESSAGES: sc::SessionMessages = sc::SessionMessages {
  ended: "Local shell process has exited",
  corrupted: "Local shell session corrupted due to internal error",
  poisoned: "Local shell session corrupted (state lock poisoned)",
  unexpected: "Local shell is not alive (unexpected state)",
};

lazy_static! {
  static ref LOCAL_SHELL_TYPE: Type = Type::object(FRAG_CC, fourCharacterCode(*b"lshl"));
  static ref LOCAL_SHELL_TYPE_VEC: Vec<Type> = vec![*LOCAL_SHELL_TYPE];
  static ref LOCAL_SHELL_VAR_TYPE: Type = Type::context_variable(&LOCAL_SHELL_TYPE_VEC);
}

// Helper: extract the shared session from a Var.
fn get_session(session_var: &Var) -> Result<&sc::ShellSession, &'static str> {
  let wrapper =
    unsafe { Var::from_ref_counted_object::<LocalShellSession>(session_var, &*LOCAL_SHELL_TYPE)? };
  Ok(unsafe { &(*wrapper).0 })
}

// ============================================================================
// LocalShell.Create Shard
// ============================================================================

#[derive(shards::shard)]
#[shard_info("LocalShell.Create", "Create a persistent local shell session")]
pub struct CreateShard {
  #[shard_required]
  required: ExposedTypes,

  #[shard_param("Shell", "Shell to use (default: /bin/bash on Unix, cmd.exe on Windows)", [common_type::string, common_type::string_var, common_type::none])]
  shell: ParamVar,

  #[shard_param("WorkingDirectory", "Initial working directory (default: current directory)", [common_type::string, common_type::string_var, common_type::none])]
  working_dir: ParamVar,

  #[shard_param("ShellArgs", "Arguments to pass to shell (default: ['--login', '-i'] for bash on Unix)", [common_type::strings, common_type::strings_var, common_type::none])]
  shell_args: ParamVar,

  #[shard_param("Rows", "PTY rows (default: 24)", [common_type::int, common_type::int_var, common_type::none])]
  rows: ParamVar,

  #[shard_param("Cols", "PTY columns (default: 80)", [common_type::int, common_type::int_var, common_type::none])]
  cols: ParamVar,

  output: ClonedVar,
}

impl Default for CreateShard {
  fn default() -> Self {
    Self {
      required: ExposedTypes::new(),
      shell: ParamVar::new(Var::default()),
      working_dir: ParamVar::new(Var::default()),
      shell_args: ParamVar::new(Var::default()),
      rows: ParamVar::new(24i64.into()),
      cols: ParamVar::new(80i64.into()),
      output: ClonedVar::default(),
    }
  }
}

#[shards::shard_impl]
impl Shard for CreateShard {
  fn input_types(&mut self) -> &Types {
    &NONE_TYPES
  }

  fn output_types(&mut self) -> &Types {
    &LOCAL_SHELL_TYPE_VEC
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

  fn activate(&mut self, context: &Context, input: &Var) -> Result<Option<Var>, &str> {
    Ok(Some(run_blocking(self, context, input)))
  }
}

impl BlockingShard for CreateShard {
  fn activate_blocking(&mut self, _context: &Context, _input: &Var) -> Result<Var, &'static str> {
    let shell_var = self.shell.get();
    let working_dir_var = self.working_dir.get();

    // Get PTY size params
    let rows_val = self.rows.get();
    let cols_val = self.cols.get();
    let rows: u16 = if rows_val.is_none() {
      24
    } else {
      let v: i64 = rows_val.as_ref().try_into().unwrap_or(24);
      v.max(1).min(u16::MAX as i64) as u16
    };
    let cols: u16 = if cols_val.is_none() {
      80
    } else {
      let v: i64 = cols_val.as_ref().try_into().unwrap_or(80);
      v.max(1).min(u16::MAX as i64) as u16
    };

    let pty_system = portable_pty::native_pty_system();

    // Determine shell command
    let shell_cmd = if !shell_var.is_none() {
      let shell_path: &str = shell_var.as_ref().try_into()?;
      shell_path.to_string()
    } else {
      #[cfg(unix)]
      let default_shell = "/bin/bash";
      #[cfg(windows)]
      let default_shell = "cmd.exe";

      default_shell.to_string()
    };

    shlog_trace!(
      "Creating local shell with: {} ({}x{})",
      shell_cmd,
      cols,
      rows
    );

    let pair = pty_system
      .openpty(PtySize {
        rows,
        cols,
        pixel_width: 0,
        pixel_height: 0,
      })
      .map_err(|_| "Failed to open PTY")?;

    let mut cmd = CommandBuilder::new(&shell_cmd);

    if !working_dir_var.is_none() {
      let wd: &str = working_dir_var.as_ref().try_into()?;
      cmd.cwd(wd);
    }

    let shell_args_var = self.shell_args.get();
    if !shell_args_var.is_none() {
      let args_seq: shards::types::SeqVar = shell_args_var
        .as_ref()
        .try_into()
        .map_err(|_| "ShellArgs must be a sequence of strings")?;
      for arg_var in args_seq.iter() {
        let arg_str = match arg_var.as_ref() {
          Var {
            valueType: shards::shardsc::SHType_String,
            ..
          } => {
            let s: &str = arg_var
              .as_ref()
              .try_into()
              .map_err(|_| "ShellArgs elements must be strings")?;
            s
          }
          _ => return Err("ShellArgs elements must be strings"),
        };
        cmd.arg(arg_str);
      }
    } else {
      #[cfg(unix)]
      if shell_cmd.contains("bash") {
        cmd.arg("--login");
        cmd.arg("-i");
      }
    }

    let child = pair
      .slave
      .spawn_command(cmd)
      .map_err(|_| "Failed to spawn shell")?;

    let reader = pair
      .master
      .try_clone_reader()
      .map_err(|_| "Failed to clone reader")?;
    let writer = pair
      .master
      .take_writer()
      .map_err(|_| "Failed to take writer")?;

    // Build the transport and shared session state.
    let transport: Arc<dyn sc::ShellTransport> = Arc::new(PtyTransport {
      pair: Arc::new(Mutex::new(Some(pair))),
      reader: Arc::new(Mutex::new(reader)),
      writer: Arc::new(Mutex::new(writer)),
      child: Arc::new(Mutex::new(Some(child))),
    });

    let output_buffer = Arc::new(Mutex::new(Vec::new()));
    let total_bytes_written = Arc::new(AtomicUsize::new(0));
    let is_alive = Arc::new(AtomicBool::new(true));
    let state = Arc::new(Mutex::new(sc::SessionState::Running));

    // Start reader thread BEFORE waiting for the initial prompt.
    let reader_thread = sc::start_reader_thread(
      Arc::clone(&transport),
      Arc::clone(&output_buffer),
      Arc::clone(&total_bytes_written),
      Arc::clone(&is_alive),
      Arc::clone(&state),
    );

    sc::wait_for_initial_prompt(&output_buffer)?;
    let prompt_marker = sc::setup_prompt_marker(&transport, &output_buffer, &total_bytes_written);

    let session = sc::ShellSession {
      transport,
      pending_interactive: Arc::new(Mutex::new(None)),
      is_alive,
      state,
      output_buffer,
      total_bytes_written,
      read_position: Arc::new(AtomicUsize::new(0)),
      reader_thread: Arc::new(Mutex::new(Some(reader_thread))),
      prompt_marker,
      term_rows: Arc::new(AtomicU16::new(rows)),
      term_cols: Arc::new(AtomicU16::new(cols)),
      messages: LOCAL_SHELL_MESSAGES,
    };

    let shell_var = Var::new_ref_counted(LocalShellSession(session), &*LOCAL_SHELL_TYPE);
    self.output = shell_var.into();
    Ok(self.output.0)
  }
}

// ============================================================================
// LocalShell.Execute Shard
// ============================================================================

#[derive(shards::shard)]
#[shard_info(
  "LocalShell.Execute",
  "Execute a command in the persistent local shell session"
)]
pub struct ExecuteShard {
  #[shard_required]
  required: ExposedTypes,

  #[shard_param("Session", "Local shell session object", [*LOCAL_SHELL_TYPE, *LOCAL_SHELL_VAR_TYPE])]
  session: ParamVar,

  #[shard_param("Timeout", "Command timeout in seconds (default: 30)", [common_type::int, common_type::int_var])]
  timeout_secs: ParamVar,

  #[shard_param("CleanOutput", "Clean ANSI codes and prompts from output (default: true)", [common_type::bool, common_type::bool_var])]
  clean_output: ParamVar,

  #[shard_param("MaxOutputBytes", "Maximum output bytes to retain (keeps tail, 0 = unlimited, default: 65536)", [common_type::int, common_type::int_var])]
  max_output_bytes: ParamVar,

  output: ClonedVar,
}

impl Default for ExecuteShard {
  fn default() -> Self {
    Self {
      required: ExposedTypes::new(),
      session: ParamVar::default(),
      timeout_secs: ParamVar::new(30i64.into()),
      clean_output: ParamVar::new(true.into()),
      max_output_bytes: ParamVar::new(65536i64.into()),
      output: ClonedVar::default(),
    }
  }
}

#[shards::shard_impl]
impl Shard for ExecuteShard {
  fn input_types(&mut self) -> &Types {
    &STRING_TYPES
  }

  fn output_types(&mut self) -> &Types {
    &sc::EXECUTE_OUTPUT_TYPES
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

  fn activate(&mut self, context: &Context, input: &Var) -> Result<Option<Var>, &str> {
    Ok(Some(run_blocking(self, context, input)))
  }
}

impl BlockingShard for ExecuteShard {
  fn activate_blocking(&mut self, _context: &Context, input: &Var) -> Result<Var, &'static str> {
    let cmd: &str = input.try_into()?;
    let session_var = *self.session.get();
    let should_clean: bool = self.clean_output.get().as_ref().try_into()?;
    let max_output_bytes: i64 = self.max_output_bytes.get().as_ref().try_into()?;
    let timeout_secs: i64 = self.timeout_secs.get().as_ref().try_into()?;

    let s = get_session(&session_var)?;
    self.output = sc::execute(
      s,
      cmd,
      timeout_secs,
      should_clean,
      max_output_bytes,
      "Command is waiting for input. Use LocalShell.SendInput to interact.",
    )?;
    Ok(self.output.0)
  }
}

// ============================================================================
// LocalShell.SendInput Shard
// ============================================================================

#[derive(shards::shard)]
#[shard_info(
  "LocalShell.SendInput",
  "Send input to an interactive local shell command"
)]
pub struct SendInputShard {
  #[shard_required]
  required: ExposedTypes,

  #[shard_param("Session", "Local shell session object", [*LOCAL_SHELL_TYPE, *LOCAL_SHELL_VAR_TYPE])]
  session: ParamVar,

  #[shard_param("Timeout", "Timeout in seconds (default: 10)", [common_type::int, common_type::int_var])]
  timeout_secs: ParamVar,

  #[shard_param("MaxOutputBytes", "Maximum output bytes to retain (keeps tail, 0 = unlimited, default: 65536)", [common_type::int, common_type::int_var])]
  max_output_bytes: ParamVar,

  output: ClonedVar,
}

impl Default for SendInputShard {
  fn default() -> Self {
    Self {
      required: ExposedTypes::new(),
      session: ParamVar::default(),
      timeout_secs: ParamVar::new(10i64.into()),
      max_output_bytes: ParamVar::new(65536i64.into()),
      output: ClonedVar::default(),
    }
  }
}

#[shards::shard_impl]
impl Shard for SendInputShard {
  fn input_types(&mut self) -> &Types {
    &STRING_TYPES
  }

  fn output_types(&mut self) -> &Types {
    &sc::EXECUTE_OUTPUT_TYPES
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

  fn activate(&mut self, context: &Context, input: &Var) -> Result<Option<Var>, &str> {
    Ok(Some(run_blocking(self, context, input)))
  }
}

impl BlockingShard for SendInputShard {
  fn activate_blocking(&mut self, _context: &Context, input: &Var) -> Result<Var, &'static str> {
    let input_str: &str = input.try_into()?;
    let session_var = *self.session.get();
    let max_output_bytes: i64 = self.max_output_bytes.get().as_ref().try_into()?;
    let timeout_secs: i64 = self.timeout_secs.get().as_ref().try_into()?;

    let s = get_session(&session_var)?;
    self.output = sc::send_input(
      s,
      input_str,
      timeout_secs,
      max_output_bytes,
      "Command still running. Use LocalShell.SendInput to continue.",
    )?;
    Ok(self.output.0)
  }
}

// ============================================================================
// LocalShell.IsAlive Shard
// ============================================================================

#[derive(shards::shard)]
#[shard_info("LocalShell.IsAlive", "Check if local shell session is still alive")]
pub struct IsAliveShard {
  #[shard_required]
  required: ExposedTypes,

  output: ClonedVar,
}

impl Default for IsAliveShard {
  fn default() -> Self {
    Self {
      required: ExposedTypes::new(),
      output: ClonedVar::default(),
    }
  }
}

#[shards::shard_impl]
impl Shard for IsAliveShard {
  fn input_types(&mut self) -> &Types {
    &LOCAL_SHELL_TYPE_VEC
  }

  fn output_types(&mut self) -> &Types {
    &BOOL_TYPES
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
    Ok(common_type::bool)
  }

  fn activate(&mut self, _context: &Context, input: &Var) -> Result<Option<Var>, &str> {
    let s = get_session(input)?;
    let is_alive = s.is_alive.load(Ordering::Acquire);
    self.output = is_alive.into();
    Ok(Some(self.output.0))
  }
}

// ============================================================================
// LocalShell.Read Shard — raw non-blocking read
// ============================================================================

#[derive(shards::shard)]
#[shard_info(
  "LocalShell.Read",
  "Read raw output from local shell session (non-blocking)"
)]
pub struct ReadShard {
  #[shard_required]
  required: ExposedTypes,

  #[shard_param("Session", "Local shell session object", [*LOCAL_SHELL_TYPE, *LOCAL_SHELL_VAR_TYPE])]
  session: ParamVar,

  #[shard_param("MaxBytes", "Maximum bytes to read (default: 65536)", [common_type::int, common_type::int_var])]
  max_bytes: ParamVar,

  #[shard_param("StripAnsi", "Strip ANSI escape sequences (default: false)", [common_type::bool, common_type::bool_var])]
  strip_ansi: ParamVar,

  #[shard_param("Timeout", "Timeout in milliseconds, 0 = immediate (default: 0)", [common_type::int, common_type::int_var])]
  timeout_ms: ParamVar,

  output: ClonedVar,
}

impl Default for ReadShard {
  fn default() -> Self {
    Self {
      required: ExposedTypes::new(),
      session: ParamVar::default(),
      max_bytes: ParamVar::new(65536i64.into()),
      strip_ansi: ParamVar::new(false.into()),
      timeout_ms: ParamVar::new(0i64.into()),
      output: ClonedVar::default(),
    }
  }
}

#[shards::shard_impl]
impl Shard for ReadShard {
  fn input_types(&mut self) -> &Types {
    &NONE_TYPES
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

  fn activate(&mut self, context: &Context, _input: &Var) -> Result<Option<Var>, &str> {
    let session_var = *self.session.get();
    let max_bytes: i64 = self.max_bytes.get().as_ref().try_into()?;
    let strip: bool = self.strip_ansi.get().as_ref().try_into()?;
    let timeout_ms: i64 = self.timeout_ms.get().as_ref().try_into()?;

    let s = get_session(&session_var)?;
    let output_str = sc::read_raw(s, context, max_bytes, strip, timeout_ms)?;

    self.output = Var::ephemeral_string(&output_str).into();
    Ok(Some(self.output.0))
  }
}

// ============================================================================
// LocalShell.Write Shard — raw byte write
// ============================================================================

#[derive(shards::shard)]
#[shard_info("LocalShell.Write", "Write raw bytes to local shell session")]
pub struct WriteShard {
  #[shard_required]
  required: ExposedTypes,

  #[shard_param("Session", "Local shell session object", [*LOCAL_SHELL_TYPE, *LOCAL_SHELL_VAR_TYPE])]
  session: ParamVar,

  #[shard_param("AppendNewline", "Append newline after input (default: false)", [common_type::bool, common_type::bool_var])]
  append_newline: ParamVar,

  output: ClonedVar,
}

impl Default for WriteShard {
  fn default() -> Self {
    Self {
      required: ExposedTypes::new(),
      session: ParamVar::default(),
      append_newline: ParamVar::new(false.into()),
      output: ClonedVar::default(),
    }
  }
}

#[shards::shard_impl]
impl Shard for WriteShard {
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
    let input_str: &str = input.try_into()?;
    let session_var = *self.session.get();
    let append_nl: bool = self.append_newline.get().as_ref().try_into()?;

    let s = get_session(&session_var)?;
    sc::write_raw(s, input_str, append_nl)?;

    // Passthrough input
    self.output = input.into();
    Ok(Some(self.output.0))
  }
}

// ============================================================================
// LocalShell.Resize Shard — PTY resize
// ============================================================================

#[derive(shards::shard)]
#[shard_info(
  "LocalShell.Resize",
  "Resize the PTY terminal of a local shell session"
)]
pub struct ResizeShard {
  #[shard_required]
  required: ExposedTypes,

  #[shard_param("Session", "Local shell session object", [*LOCAL_SHELL_TYPE, *LOCAL_SHELL_VAR_TYPE])]
  session: ParamVar,

  #[shard_param("Rows", "Number of rows", [common_type::int, common_type::int_var])]
  rows: ParamVar,

  #[shard_param("Cols", "Number of columns", [common_type::int, common_type::int_var])]
  cols: ParamVar,

  output: ClonedVar,
}

impl Default for ResizeShard {
  fn default() -> Self {
    Self {
      required: ExposedTypes::new(),
      session: ParamVar::default(),
      rows: ParamVar::new(24i64.into()),
      cols: ParamVar::new(80i64.into()),
      output: ClonedVar::default(),
    }
  }
}

#[shards::shard_impl]
impl Shard for ResizeShard {
  fn input_types(&mut self) -> &Types {
    &NONE_TYPES
  }

  fn output_types(&mut self) -> &Types {
    &NONE_TYPES
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

  fn activate(&mut self, _context: &Context, _input: &Var) -> Result<Option<Var>, &str> {
    let session_var = *self.session.get();
    let rows_val: i64 = self.rows.get().as_ref().try_into()?;
    let cols_val: i64 = self.cols.get().as_ref().try_into()?;

    let s = get_session(&session_var)?;
    sc::resize(s, rows_val, cols_val)?;

    Ok(None)
  }
}

// ============================================================================
// LocalShell.WaitFor Shard — wait until output matches a regex
// ============================================================================

#[derive(shards::shard)]
#[shard_info(
  "LocalShell.WaitFor",
  "Wait until the session output matches a regular expression, returning the matched text (errors on timeout)"
)]
pub struct WaitForShard {
  #[shard_required]
  required: ExposedTypes,

  #[shard_param("Session", "Local shell session object", [*LOCAL_SHELL_TYPE, *LOCAL_SHELL_VAR_TYPE])]
  session: ParamVar,

  #[shard_param("Pattern", "Regular expression to wait for in the output", [common_type::string, common_type::string_var])]
  pattern: ParamVar,

  #[shard_param("Timeout", "Timeout in milliseconds, 0 = wait indefinitely (default: 30000)", [common_type::int, common_type::int_var])]
  timeout_ms: ParamVar,

  #[shard_param("StripAnsi", "Strip ANSI escape sequences before matching (default: true)", [common_type::bool, common_type::bool_var])]
  strip_ansi: ParamVar,

  output: ClonedVar,
}

impl Default for WaitForShard {
  fn default() -> Self {
    Self {
      required: ExposedTypes::new(),
      session: ParamVar::default(),
      pattern: ParamVar::default(),
      timeout_ms: ParamVar::new(30000i64.into()),
      strip_ansi: ParamVar::new(true.into()),
      output: ClonedVar::default(),
    }
  }
}

#[shards::shard_impl]
impl Shard for WaitForShard {
  fn input_types(&mut self) -> &Types {
    &NONE_TYPES
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

  fn activate(&mut self, context: &Context, _input: &Var) -> Result<Option<Var>, &str> {
    let session_var = *self.session.get();
    let pattern: &str = self.pattern.get().as_ref().try_into()?;
    let timeout_ms: i64 = self.timeout_ms.get().as_ref().try_into()?;
    let strip: bool = self.strip_ansi.get().as_ref().try_into()?;

    let s = get_session(&session_var)?;
    let matched = sc::wait_for(s, context, pattern, timeout_ms, strip)?;

    self.output = Var::ephemeral_string(&matched).into();
    Ok(Some(self.output.0))
  }
}

// ============================================================================
// Module Registration
// ============================================================================

#[no_mangle]
pub extern "C" fn shardsRegister_localshell_rust(core: *mut shards::shardsc::SHCore) {
  unsafe {
    shards::core::Core = core;
  }

  let mut info = shards::SHObjectInfo::default();
  info.name = cstr!("LocalShell.Session").as_ptr() as shards::SHString;
  shards::core::register_object_type_internal(FRAG_CC, fourCharacterCode(*b"lshl"), info);

  register_shard::<CreateShard>();
  register_shard::<ExecuteShard>();
  register_shard::<SendInputShard>();
  register_shard::<IsAliveShard>();
  register_shard::<ReadShard>();
  register_shard::<WriteShard>();
  register_shard::<ResizeShard>();
  register_shard::<WaitForShard>();

  shlog_trace!("LocalShell module registered");
}
