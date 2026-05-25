/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2025 Fragcolor Pte. Ltd. */

//! SSH module: persistent, interactive remote shell sessions over SSH. All the
//! session/buffer/prompt machinery lives in `shards-shell-common`; this crate
//! only provides the SSH transport and the `SSH.*` shards.

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

use ssh2::Session;
use std::io::prelude::*;
use std::net::{TcpStream, ToSocketAddrs};
use std::path::Path;
use std::sync::atomic::{AtomicBool, AtomicU16, AtomicUsize, Ordering};
use std::sync::{Arc, Mutex};
use std::time::Duration;

// Helper: check if an IO error indicates connection loss.
fn is_connection_error(err: &std::io::Error) -> bool {
  use std::io::ErrorKind;
  matches!(
    err.kind(),
    ErrorKind::ConnectionReset
      | ErrorKind::ConnectionAborted
      | ErrorKind::BrokenPipe
      | ErrorKind::UnexpectedEof
  )
}

// ============================================================================
// SSH transport
// ============================================================================

struct SshTransport {
  session: Arc<Mutex<Session>>,
  channel: Arc<Mutex<ssh2::Channel>>,
}

impl sc::ShellTransport for SshTransport {
  fn read_into(&self, buf: &mut [u8]) -> sc::ReadOutcome {
    // Ensure non-blocking mode, then RELEASE the session lock before touching
    // the channel. `write_all` holds the channel lock while acquiring the
    // session lock, so the reader must never hold session while waiting on
    // channel — otherwise the two could deadlock.
    if let Ok(sess) = self.session.lock() {
      sess.set_blocking(false);
    }

    let read_result = match self.channel.lock() {
      Ok(mut ch) => ch.read(buf),
      Err(_) => return sc::ReadOutcome::Ended,
    };

    match read_result {
      Ok(n) if n > 0 => sc::ReadOutcome::Data(n),
      Ok(_) => sc::ReadOutcome::Ended,
      Err(e) if e.kind() == std::io::ErrorKind::WouldBlock => sc::ReadOutcome::Retry,
      Err(e) if is_connection_error(&e) => sc::ReadOutcome::Ended,
      Err(e) => {
        shlog_trace!("SSH reader thread: read error (non-fatal): {}", e);
        sc::ReadOutcome::Retry
      }
    }
  }

  fn write_all(&self, data: &[u8]) -> Result<(), sc::TransportError> {
    let mut channel = self
      .channel
      .lock()
      .map_err(|_| sc::TransportError::Other("Channel lock poisoned"))?;

    // Set blocking for writes to ensure they complete.
    if let Ok(sess) = self.session.lock() {
      sess.set_blocking(true);
    }

    if let Err(e) = channel.write_all(data) {
      // Restore non-blocking before reporting.
      if let Ok(sess) = self.session.lock() {
        sess.set_blocking(false);
      }
      if is_connection_error(&e) {
        return Err(sc::TransportError::ConnectionLost);
      }
      return Err(sc::TransportError::Other("Failed to write to SSH channel"));
    }

    let _ = channel.flush();

    // Restore non-blocking.
    if let Ok(sess) = self.session.lock() {
      sess.set_blocking(false);
    }

    Ok(())
  }

  fn resize(&self, rows: u16, cols: u16) -> Result<(), &'static str> {
    // Set blocking for the resize request.
    if let Ok(sess) = self.session.lock() {
      sess.set_blocking(true);
    }

    {
      let mut channel = self.channel.lock().map_err(|_| "Channel lock poisoned")?;
      channel
        .request_pty_size(cols as u32, rows as u32, None, None)
        .map_err(|_| "Failed to resize PTY")?;
    }

    // Restore non-blocking.
    if let Ok(sess) = self.session.lock() {
      sess.set_blocking(false);
    }

    Ok(())
  }

  fn close(&self) {
    // Close channel gracefully.
    if let Ok(mut channel) = self.channel.lock() {
      let _ = channel.send_eof();
      let _ = channel.wait_eof();
      let _ = channel.close();
      let _ = channel.wait_close();
    }

    // Disconnect session.
    if let Ok(session) = self.session.lock() {
      let _ = session.disconnect(None, "Session closed", None);
    }
  }
}

// ============================================================================
// Session object wrapper (local newtype satisfies the orphan rule for the
// ref-counted object type impl).
// ============================================================================

mod ssh_shell {
  use super::*;

  pub struct SSHShell(pub sc::ShellSession);

  ref_counted_object_type_impl!(SSHShell);
}

use ssh_shell::*;

const SSH_MESSAGES: sc::SessionMessages = sc::SessionMessages {
  ended: "SSH connection lost",
  corrupted: "SSH session corrupted due to internal error",
  poisoned: "SSH session corrupted (state lock poisoned)",
  unexpected: "SSH connection is not alive (unexpected state)",
};

lazy_static! {
  static ref SSH_SHELL_TYPE: Type = Type::object(FRAG_CC, fourCharacterCode(*b"sshs"));
  static ref SSH_SHELL_TYPE_VEC: Vec<Type> = vec![*SSH_SHELL_TYPE];
  static ref SSH_SHELL_VAR_TYPE: Type = Type::context_variable(&SSH_SHELL_TYPE_VEC);
}

// Helper: extract the shared session from a Var.
fn get_session(session_var: &Var) -> Result<&sc::ShellSession, &'static str> {
  let wrapper = unsafe { Var::from_ref_counted_object::<SSHShell>(session_var, &*SSH_SHELL_TYPE)? };
  Ok(unsafe { &(*wrapper).0 })
}

// ============================================================================
// SSH.Connect Shard
// ============================================================================

#[derive(shards::shard)]
#[shard_info(
  "SSH.Connect",
  "Connect to an SSH server and create a persistent shell session"
)]
pub struct ConnectShard {
  #[shard_required]
  required: ExposedTypes,

  #[shard_param("Host", "SSH server hostname or IP address", [common_type::string, common_type::string_var])]
  host: ParamVar,

  #[shard_param("Port", "SSH server port (default: 22)", [common_type::int, common_type::int_var])]
  port: ParamVar,

  #[shard_param("User", "SSH username", [common_type::string, common_type::string_var])]
  user: ParamVar,

  #[shard_param("KeyPath", "Path to SSH private key file", [common_type::string, common_type::string_var, common_type::none])]
  key_path: ParamVar,

  #[shard_param("Password", "SSH password (if not using key)", [common_type::string, common_type::string_var, common_type::none])]
  password: ParamVar,

  #[shard_param("Timeout", "Connection timeout in seconds (default: 10)", [common_type::int, common_type::int_var])]
  timeout_secs: ParamVar,

  #[shard_param("Rows", "PTY rows (default: 24)", [common_type::int, common_type::int_var, common_type::none])]
  rows: ParamVar,

  #[shard_param("Cols", "PTY columns (default: 80)", [common_type::int, common_type::int_var, common_type::none])]
  cols: ParamVar,

  output: ClonedVar,
}

impl Default for ConnectShard {
  fn default() -> Self {
    Self {
      required: ExposedTypes::new(),
      host: ParamVar::new(Var::ephemeral_string("localhost")),
      port: ParamVar::new(22i64.into()),
      user: ParamVar::new(Var::ephemeral_string("user")),
      key_path: ParamVar::new(Var::default()),
      password: ParamVar::new(Var::default()),
      timeout_secs: ParamVar::new(10i64.into()),
      rows: ParamVar::new(24i64.into()),
      cols: ParamVar::new(80i64.into()),
      output: ClonedVar::default(),
    }
  }
}

#[shards::shard_impl]
impl Shard for ConnectShard {
  fn input_types(&mut self) -> &Types {
    &NONE_TYPES
  }

  fn output_types(&mut self) -> &Types {
    &SSH_SHELL_TYPE_VEC
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

impl BlockingShard for ConnectShard {
  fn activate_blocking(&mut self, _context: &Context, _input: &Var) -> Result<Var, &'static str> {
    let host: &str = self.host.get().as_ref().try_into()?;
    let port: i64 = self.port.get().as_ref().try_into()?;
    let user: &str = self.user.get().as_ref().try_into()?;
    let timeout_secs: i64 = self.timeout_secs.get().as_ref().try_into()?;

    let key_path_var = self.key_path.get();
    let password_var = self.password.get();

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

    // Connect to SSH server
    let addr = format!("{}:{}", host, port);

    let socket_addrs: Vec<_> = addr
      .to_socket_addrs()
      .map_err(|_| "Failed to resolve hostname")?
      .collect();

    let socket_addr = socket_addrs
      .first()
      .ok_or("No address found for hostname")?;

    let tcp = TcpStream::connect_timeout(socket_addr, Duration::from_secs(timeout_secs as u64))
      .map_err(|_| "Connection failed")?;

    tcp
      .set_read_timeout(Some(Duration::from_secs(timeout_secs as u64)))
      .map_err(|_| "Failed to set timeout")?;

    let mut sess = Session::new().map_err(|_| "Failed to create SSH session")?;
    sess.set_tcp_stream(tcp);
    sess.handshake().map_err(|_| "SSH handshake failed")?;

    // Authenticate
    if !key_path_var.is_none() {
      let key_path: &str = key_path_var.as_ref().try_into()?;
      let key_path_expanded = shellexpand::tilde(key_path).to_string();
      sess
        .userauth_pubkey_file(user, None, Path::new(&key_path_expanded), None)
        .map_err(|_| "Public key authentication failed")?;
    } else if !password_var.is_none() {
      let password: &str = password_var.as_ref().try_into()?;
      sess
        .userauth_password(user, password)
        .map_err(|_| "Password authentication failed")?;
    } else {
      return Err("Either KeyPath or Password must be provided");
    }

    if !sess.authenticated() {
      return Err("Authentication failed");
    }

    // Open channel and request PTY with size
    let mut channel = sess
      .channel_session()
      .map_err(|_| "Failed to open channel")?;
    channel
      .request_pty(
        "xterm-256color",
        None,
        Some((cols as u32, rows as u32, 0, 0)),
      )
      .map_err(|_| "Failed to request PTY")?;
    channel.shell().map_err(|_| "Failed to start shell")?;

    // Set non-blocking mode for reads
    sess.set_blocking(false);

    // Build the transport and shared session state.
    let transport: Arc<dyn sc::ShellTransport> = Arc::new(SshTransport {
      session: Arc::new(Mutex::new(sess)),
      channel: Arc::new(Mutex::new(channel)),
    });

    let output_buffer = Arc::new(Mutex::new(Vec::new()));
    let total_bytes_written = Arc::new(AtomicUsize::new(0));
    let is_connected = Arc::new(AtomicBool::new(true));
    let state = Arc::new(Mutex::new(sc::SessionState::Running));

    // Start reader thread BEFORE waiting for the initial prompt.
    let reader_thread = sc::start_reader_thread(
      Arc::clone(&transport),
      Arc::clone(&output_buffer),
      Arc::clone(&total_bytes_written),
      Arc::clone(&is_connected),
      Arc::clone(&state),
    );

    sc::wait_for_initial_prompt(&output_buffer)?;

    // Try to switch to bash for consistent behavior across servers.
    shlog_trace!("Shell ready, attempting to switch to bash for consistent behavior");
    let _ = transport.write_all(b"command -v bash >/dev/null 2>&1 && exec bash --login\n");

    // Wait for bash to start and show a prompt.
    std::thread::sleep(Duration::from_millis(500));

    let mut bash_switched = false;
    for _ in 0..10 {
      let buf = output_buffer.lock().map_err(|_| "Buffer lock poisoned")?;
      let output_str = String::from_utf8_lossy(&buf);
      if sc::is_prompt(&output_str) {
        bash_switched =
          !output_str.contains("command not found") && !output_str.contains("not found");
        break;
      }
      drop(buf);
      std::thread::sleep(Duration::from_millis(sc::ITERATION_SLEEP_MS));
    }

    if bash_switched {
      shlog_trace!("Switched to bash");
    } else {
      shlog_trace!("Bash not available or switch failed, continuing with current shell");
    }

    let prompt_marker = sc::setup_prompt_marker(&transport, &output_buffer, &total_bytes_written);

    shlog_trace!("SSH connection established");

    let session = sc::ShellSession {
      transport,
      pending_interactive: Arc::new(Mutex::new(None)),
      is_alive: is_connected,
      state,
      output_buffer,
      total_bytes_written,
      read_position: Arc::new(AtomicUsize::new(0)),
      reader_thread: Arc::new(Mutex::new(Some(reader_thread))),
      prompt_marker,
      term_rows: Arc::new(AtomicU16::new(rows)),
      term_cols: Arc::new(AtomicU16::new(cols)),
      messages: SSH_MESSAGES,
    };

    let shell_var = Var::new_ref_counted(SSHShell(session), &*SSH_SHELL_TYPE);
    self.output = shell_var.into();
    Ok(self.output.0)
  }
}

// ============================================================================
// SSH.Execute Shard
// ============================================================================

#[derive(shards::shard)]
#[shard_info("SSH.Execute", "Execute a command in the persistent SSH shell session")]
pub struct ExecuteShard {
  #[shard_required]
  required: ExposedTypes,

  #[shard_param("Session", "SSH shell session object", [*SSH_SHELL_TYPE, *SSH_SHELL_VAR_TYPE])]
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
      "Command is waiting for input. Use SSH.SendInput to interact.",
    )?;
    Ok(self.output.0)
  }
}

// ============================================================================
// SSH.SendInput Shard
// ============================================================================

#[derive(shards::shard)]
#[shard_info("SSH.SendInput", "Send input to an interactive SSH command")]
pub struct SendInputShard {
  #[shard_required]
  required: ExposedTypes,

  #[shard_param("Session", "SSH shell session object", [*SSH_SHELL_TYPE, *SSH_SHELL_VAR_TYPE])]
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
      "Command still running. Use SSH.SendInput to continue.",
    )?;
    Ok(self.output.0)
  }
}

// ============================================================================
// SSH.IsConnected Shard
// ============================================================================

#[derive(shards::shard)]
#[shard_info("SSH.IsConnected", "Check if SSH session is still connected")]
pub struct IsConnectedShard {
  #[shard_required]
  required: ExposedTypes,

  output: ClonedVar,
}

impl Default for IsConnectedShard {
  fn default() -> Self {
    Self {
      required: ExposedTypes::new(),
      output: ClonedVar::default(),
    }
  }
}

#[shards::shard_impl]
impl Shard for IsConnectedShard {
  fn input_types(&mut self) -> &Types {
    &SSH_SHELL_TYPE_VEC
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
    let is_connected = s.is_alive.load(Ordering::Acquire);
    self.output = is_connected.into();
    Ok(Some(self.output.0))
  }
}

// ============================================================================
// SSH.Read Shard — raw non-blocking read
// ============================================================================

#[derive(shards::shard)]
#[shard_info("SSH.Read", "Read raw output from SSH session (non-blocking)")]
pub struct ReadShard {
  #[shard_required]
  required: ExposedTypes,

  #[shard_param("Session", "SSH shell session object", [*SSH_SHELL_TYPE, *SSH_SHELL_VAR_TYPE])]
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
// SSH.Write Shard — raw byte write
// ============================================================================

#[derive(shards::shard)]
#[shard_info("SSH.Write", "Write raw bytes to SSH session")]
pub struct WriteShard {
  #[shard_required]
  required: ExposedTypes,

  #[shard_param("Session", "SSH shell session object", [*SSH_SHELL_TYPE, *SSH_SHELL_VAR_TYPE])]
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
// SSH.Resize Shard — PTY resize
// ============================================================================

#[derive(shards::shard)]
#[shard_info("SSH.Resize", "Resize the PTY terminal of an SSH session")]
pub struct ResizeShard {
  #[shard_required]
  required: ExposedTypes,

  #[shard_param("Session", "SSH shell session object", [*SSH_SHELL_TYPE, *SSH_SHELL_VAR_TYPE])]
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
// Module Registration
// ============================================================================

#[no_mangle]
pub extern "C" fn shardsRegister_ssh_rust(core: *mut shards::shardsc::SHCore) {
  unsafe {
    shards::core::Core = core;
  }

  // Register SSHShell object type
  let mut info = shards::SHObjectInfo::default();
  info.name = cstr!("SSH.Shell").as_ptr() as shards::SHString;
  shards::core::register_object_type_internal(FRAG_CC, fourCharacterCode(*b"sshs"), info);

  // Register shards
  register_shard::<ConnectShard>();
  register_shard::<ExecuteShard>();
  register_shard::<SendInputShard>();
  register_shard::<IsConnectedShard>();
  register_shard::<ReadShard>();
  register_shard::<WriteShard>();
  register_shard::<ResizeShard>();

  shlog_trace!("SSH module registered");
}
