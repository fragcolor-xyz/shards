/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2025 Fragcolor Pte. Ltd. */

//! Shared machinery for Shards interactive shell-session modules.
//!
//! Both `shards-localshell` (PTY-backed local shells) and `shards-ssh` (remote
//! shells over SSH) implement the exact same "persistent interactive shell
//! session" model: a background reader thread fills a shared byte buffer, a
//! monotonic byte counter tracks progress (immune to buffer truncation), and a
//! sentinel `PS1` marker drives reliable prompt detection.
//!
//! This crate owns everything except the transport. A module provides an
//! implementation of [`ShellTransport`] (how to read/write/resize/close its
//! underlying channel) plus its `Create`/`Connect` shard, and delegates the
//! `Execute` / `SendInput` / `Read` / `Write` / `Resize` logic to the free
//! functions here.

#![allow(non_upper_case_globals)]
#![allow(non_camel_case_types)]
#![allow(non_snake_case)]

#[macro_use]
extern crate shards;

#[macro_use]
extern crate lazy_static;

use shards::types::common_type;
use shards::types::AutoTableVar;
use shards::types::ClonedVar;
use shards::types::Context;
use shards::types::Type;
use shards::types::Var;

use std::sync::atomic::{AtomicBool, AtomicU16, AtomicUsize, Ordering};
use std::sync::{Arc, Mutex};
use std::time::Duration;

// ============================================================================
// Configuration constants (shared by every shell transport)
// ============================================================================
pub const INITIAL_PROMPT_WAIT_MS: u64 = 500;
pub const INITIAL_PROMPT_MAX_RETRIES: usize = 20;
pub const COMMAND_OUTPUT_WAIT_MS: u64 = 200;
pub const ITERATION_SLEEP_MS: u64 = 100;
pub const INTERACTIVE_DETECTION_ITERATIONS: usize = 30; // 30 * 100ms = 3 seconds
pub const SENDINPUT_INITIAL_WAIT_MS: u64 = 500;
pub const SENDINPUT_NO_DATA_THRESHOLD: usize = 20; // 20 iterations = 2 seconds
pub const MAX_BUFFER_BYTES: usize = 65536; // 64KB default
pub const BUFFER_TRUNCATE_KEEP_RATIO: usize = 93; // Keep 93% on truncation
pub const READER_POLL_MS: u64 = 50; // How often a polling reader thread retries

lazy_static! {
    /// Output type shared by `Execute` / `SendInput`: a string-keyed table.
    pub static ref EXECUTE_OUTPUT_TYPES: Vec<Type> = vec![common_type::string_table];
}

// ============================================================================
// Transport abstraction
// ============================================================================

/// Result of a single non-/blocking read attempt by the reader thread.
pub enum ReadOutcome {
  /// `n` bytes were read into the provided buffer.
  Data(usize),
  /// The transport ended (EOF / connection closed / fatal error). The reader
  /// thread will mark the session as ended and exit.
  Ended,
  /// No data was available right now (or a transient, recoverable error). The
  /// reader thread will sleep for `poll_interval_ms()` and try again.
  Retry,
}

/// Error returned from a write attempt.
pub enum TransportError {
  /// The underlying connection/process went away. The session will be marked
  /// ended.
  ConnectionLost,
  /// Any other failure; the static string is surfaced to the caller as-is.
  Other(&'static str),
}

/// The per-module transport. All methods use `&self` + interior mutability so
/// the transport can be shared (via `Arc`) between the reader thread and the
/// activating shard.
pub trait ShellTransport: Send + Sync {
  /// Read whatever is currently available into `buf`. Called only from the
  /// reader thread.
  fn read_into(&self, buf: &mut [u8]) -> ReadOutcome;

  /// Write all bytes, blocking until they are flushed.
  fn write_all(&self, data: &[u8]) -> Result<(), TransportError>;

  /// Resize the remote/local PTY.
  fn resize(&self, rows: u16, cols: u16) -> Result<(), &'static str>;

  /// Tear down the transport (kill child / close channel / disconnect). Called
  /// once from [`ShellSession`]'s `Drop`, before the reader thread is joined.
  fn close(&self);

  /// Poll interval used when [`read_into`](Self::read_into) returns
  /// [`ReadOutcome::Retry`].
  fn poll_interval_ms(&self) -> u64 {
    READER_POLL_MS
  }
}

// ============================================================================
// Session state + messages
// ============================================================================

#[derive(Debug, Clone, PartialEq)]
pub enum SessionState {
  Running,
  /// The process exited / the connection was lost.
  Ended,
  /// An internal invariant was violated (e.g. a poisoned lock).
  Corrupted(String),
}

#[derive(Clone)]
pub struct InteractiveState {
  #[allow(dead_code)] // Stored for debugging/future use
  pub original_cmd: String,
}

/// User-facing error strings, customised per module so a dead session reports
/// "Local shell process has exited" vs "SSH connection lost".
#[derive(Clone, Copy)]
pub struct SessionMessages {
  pub ended: &'static str,
  pub corrupted: &'static str,
  pub poisoned: &'static str,
  pub unexpected: &'static str,
}

/// The transport-agnostic interactive shell session. Modules wrap this in a
/// local newtype to satisfy the orphan rule when implementing the ref-counted
/// object type.
pub struct ShellSession {
  pub transport: Arc<dyn ShellTransport>,
  pub pending_interactive: Arc<Mutex<Option<InteractiveState>>>,
  pub is_alive: Arc<AtomicBool>,
  pub state: Arc<Mutex<SessionState>>,
  pub output_buffer: Arc<Mutex<Vec<u8>>>,
  pub total_bytes_written: Arc<AtomicUsize>,
  /// Read cursor for `Read` — how many bytes have been consumed by raw reads.
  /// Lives on the session so multiple `Read` shard instances share state.
  pub read_position: Arc<AtomicUsize>,
  pub reader_thread: Arc<Mutex<Option<std::thread::JoinHandle<()>>>>,
  pub prompt_marker: Option<String>,
  pub term_rows: Arc<AtomicU16>,
  pub term_cols: Arc<AtomicU16>,
  pub messages: SessionMessages,
}

impl Drop for ShellSession {
  fn drop(&mut self) {
    shlog_trace!("Dropping ShellSession, cleaning up");

    // Step 1: Mark as not alive (prevents new operations / stops the loop).
    self.is_alive.store(false, Ordering::Release);

    // Step 2: Transport-specific teardown. For a PTY this kills the child and
    // drops the pair, which causes the blocking read in the reader thread to
    // return EOF. For SSH it closes the channel and disconnects the session.
    self.transport.close();

    // Step 3: Wait for the reader thread with a timeout.
    if let Ok(mut thread_opt) = self.reader_thread.lock() {
      if let Some(thread) = thread_opt.take() {
        shlog_trace!("Waiting for reader thread to finish");

        let mut finished = false;
        for _i in 0..20 {
          if thread.is_finished() {
            finished = true;
            break;
          }
          std::thread::sleep(Duration::from_millis(100));
        }

        if finished {
          let _ = thread.join();
          shlog_trace!("Reader thread joined successfully");
        } else {
          shlog_error!(
            "Reader thread did not finish within timeout, leaving it detached (thread leak)"
          );
        }
      }
    }

    shlog_trace!("ShellSession cleanup complete");
  }
}

// ============================================================================
// Reader thread
// ============================================================================

/// Spawn the background reader thread for a session's shared buffer. Started
/// before the initial prompt is awaited so no throwaway threads are needed.
pub fn start_reader_thread(
  transport: Arc<dyn ShellTransport>,
  buffer: Arc<Mutex<Vec<u8>>>,
  total_bytes: Arc<AtomicUsize>,
  alive: Arc<AtomicBool>,
  state: Arc<Mutex<SessionState>>,
) -> std::thread::JoinHandle<()> {
  let poll_ms = transport.poll_interval_ms();
  std::thread::spawn(move || {
    shlog_trace!("Reader thread started");
    let mut buf = [0u8; 4096];

    loop {
      if !alive.load(Ordering::Acquire) {
        shlog_trace!("Reader thread exiting (not alive)");
        break;
      }

      match transport.read_into(&mut buf) {
        ReadOutcome::Data(n) if n > 0 => match buffer.lock() {
          Ok(mut b) => {
            b.extend_from_slice(&buf[..n]);

            if b.len() > MAX_BUFFER_BYTES {
              if truncate_to_tail(&mut b, MAX_BUFFER_BYTES) {
                shlog_trace!("Reader thread: buffer truncated to {} bytes", b.len());
              }
            }

            // Increment the monotonic counter AFTER the write so that
            // a reader observing the counter always sees the bytes.
            total_bytes.fetch_add(n, Ordering::Release);
            shlog_trace!(
              "Reader thread: read {} bytes, buffer now {} bytes",
              n,
              b.len()
            );
          }
          Err(e) => {
            shlog_trace!("Reader thread: buffer lock poisoned: {}", e);
            break;
          }
        },
        ReadOutcome::Data(_) => {
          // Zero-length read: treat as ended (defensive; transports
          // normally return Ended for this).
          alive.store(false, Ordering::Release);
          if let Ok(mut s) = state.lock() {
            *s = SessionState::Ended;
          }
          shlog_trace!("Reader thread: zero-length read, ending");
          break;
        }
        ReadOutcome::Ended => {
          alive.store(false, Ordering::Release);
          if let Ok(mut s) = state.lock() {
            *s = SessionState::Ended;
          }
          shlog_trace!("Reader thread: transport ended");
          break;
        }
        ReadOutcome::Retry => {
          std::thread::sleep(Duration::from_millis(poll_ms));
        }
      }
    }
    shlog_trace!("Reader thread finished");
  })
}

// ============================================================================
// Text helpers (pure)
// ============================================================================

/// Detect shell prompts (no sentinel marker).
pub fn is_prompt(text: &str) -> bool {
  is_prompt_or_marker(text, None)
}

/// Detect shell prompts, preferring an explicit sentinel marker when present.
pub fn is_prompt_or_marker(text: &str, marker: Option<&str>) -> bool {
  let stripped_bytes = strip_ansi_escapes::strip(text.as_bytes());
  let cleaned = String::from_utf8_lossy(&stripped_bytes);

  let lines: Vec<&str> = cleaned.lines().collect();
  if let Some(last) = lines.last() {
    let trimmed = last.trim();

    // Check sentinel marker first (most reliable).
    if let Some(m) = marker {
      // When sentinel marker is set, ONLY trust the sentinel. Generic
      // prompt patterns ($ # > %) cause false positives with heredoc
      // continuation prompts and command output that happens to end with
      // these characters.
      return trimmed.contains(m);
    }

    // Exclude interactive program prompts like >>> (Python), >> (continuation).
    if trimmed.ends_with(">>>") || trimmed.ends_with(">>") {
      return false;
    }

    // Check for common shell prompt patterns (fallback when no sentinel).
    trimmed.ends_with("$ ")
      || trimmed.ends_with("$")
      || trimmed.ends_with("# ")
      || trimmed.ends_with("#")
      || trimmed.ends_with("> ")
      || trimmed.ends_with(">")
      || trimmed.ends_with("% ")
      || trimmed.ends_with("%")
  } else {
    false
  }
}

/// Detect common interactive prompts (password, confirmation, etc.).
pub fn is_interactive_prompt(text: &str) -> bool {
  let stripped_bytes = strip_ansi_escapes::strip(text.as_bytes());
  let cleaned = String::from_utf8_lossy(&stripped_bytes);

  let lines: Vec<&str> = cleaned.lines().collect();
  if lines.is_empty() {
    return false;
  }

  let check_lines = if lines.len() > 3 {
    &lines[lines.len() - 3..]
  } else {
    &lines[..]
  };

  for line in check_lines {
    let lower = line.to_lowercase();

    if lower.contains("password:")
      || lower.contains("passphrase:")
      || lower.contains("continue?")
      || lower.contains("(y/n)")
      || lower.contains("[y/n]")
      || lower.contains("press enter")
      || lower.contains("press any key")
      || lower.contains("are you sure")
      || lower.ends_with("? ")
      || lower.ends_with(": ")
    {
      return true;
    }
  }

  false
}

/// Clean output: remove ANSI codes, control chars, and a trailing prompt line.
pub fn clean_output_with_marker(output: &str, marker: Option<&str>) -> String {
  let stripped_bytes = strip_ansi_escapes::strip(output);
  let text = String::from_utf8_lossy(&stripped_bytes);

  let no_control: String = text
    .chars()
    .filter(|c| !c.is_control() || *c == '\n' || *c == '\t')
    .collect();

  let mut lines: Vec<&str> = no_control.lines().collect();

  // Remove trailing prompt if present.
  if !lines.is_empty() {
    let last_line = lines.last().unwrap();
    if is_prompt_or_marker(last_line, marker) {
      lines.pop();
    }
  }

  let result = lines.join("\n");
  result.trim().to_string()
}

/// Truncate an output buffer to keep the tail, prepending a truncation notice.
pub fn truncate_to_tail(buffer: &mut Vec<u8>, max_bytes: usize) -> bool {
  if buffer.len() <= max_bytes {
    return false;
  }

  let truncate_msg = b"[... output truncated ...]\n";
  let keep_bytes = (max_bytes * BUFFER_TRUNCATE_KEEP_RATIO) / 100;

  // When max_bytes is too small to fit the truncation notice plus any tail,
  // just keep the last keep_bytes raw (without the notice). Otherwise the
  // skip computation below underflows and `drain` panics on an out-of-range
  // index — reachable from user code via a small MaxOutputBytes (1..=29).
  if keep_bytes <= truncate_msg.len() {
    let skip = buffer.len() - keep_bytes;
    buffer.drain(..skip);
    return true;
  }

  let tail_len = keep_bytes - truncate_msg.len();
  let skip = buffer.len() - tail_len;
  let tail: Vec<u8> = buffer.drain(skip..).collect();
  buffer.clear();
  buffer.extend_from_slice(truncate_msg);
  buffer.extend_from_slice(&tail);

  true
}

/// Render raw PTY output through a virtual VT100 terminal to get properly-spaced
/// text. Ink/TUI frameworks use cursor positioning sequences (CSI H, CSI C, etc.)
/// for layout instead of literal spaces — simple ANSI stripping concatenates
/// words without gaps. This processes bytes through a virtual screen and extracts
/// the resulting text grid.
pub fn render_through_virtual_terminal(raw_bytes: &[u8], rows: u16, cols: u16) -> String {
  let mut parser = vt100::Parser::new(rows, cols, 0);
  parser.process(raw_bytes);

  let screen = parser.screen();
  let mut output = String::new();

  for row in 0..rows {
    let row_text = screen.contents_between(row, 0, row, cols);
    let trimmed = row_text.trim_end();
    if !trimmed.is_empty() || !output.is_empty() {
      if !output.is_empty() {
        output.push('\n');
      }
      output.push_str(trimmed);
    }
  }

  output.trim().to_string()
}

/// Interpret escape sequences in input strings for PTY writing. Handles `\r`,
/// `\n`, `\t`, `\\`, `\xNN`. Also converts bare LF (0x0A) to CR (0x0D) because
/// PTY Enter = CR, and TUI apps in raw mode expect CR not LF.
pub fn interpret_escape_sequences(input: &str) -> Vec<u8> {
  let mut result = Vec::with_capacity(input.len());
  let mut chars = input.chars().peekable();

  while let Some(ch) = chars.next() {
    if ch == '\\' {
      match chars.peek() {
        Some('n') => {
          chars.next();
          result.push(0x0A);
        }
        Some('r') => {
          chars.next();
          result.push(0x0D);
        }
        Some('t') => {
          chars.next();
          result.push(0x09);
        }
        Some('\\') => {
          chars.next();
          result.push(b'\\');
        }
        Some('x') => {
          chars.next(); // consume 'x'
          let mut hex = String::new();
          for _ in 0..2 {
            if let Some(&c) = chars.peek() {
              if c.is_ascii_hexdigit() {
                hex.push(c);
                chars.next();
              } else {
                break;
              }
            }
          }
          if let Ok(byte) = u8::from_str_radix(&hex, 16) {
            result.push(byte);
          }
        }
        _ => {
          result.push(b'\\');
        }
      }
    } else if ch == '\n' {
      // Bare LF → CR: terminal Enter sends CR to PTY master.
      result.push(0x0D);
    } else {
      let mut buf = [0u8; 4];
      let encoded = ch.encode_utf8(&mut buf);
      result.extend_from_slice(encoded.as_bytes());
    }
  }

  result
}

// ============================================================================
// Session helpers
// ============================================================================

/// Mark a session corrupted after an unrecoverable internal error.
pub fn mark_session_corrupted(s: &ShellSession, error_context: &str) {
  shlog_error!(
    "Critical error in shell session ({}), marking as corrupted",
    error_context
  );
  s.is_alive.store(false, Ordering::Release);
  if let Ok(mut state) = s.state.lock() {
    *state = SessionState::Corrupted(error_context.to_string());
  }
}

/// Return a descriptive error if the session is no longer alive.
pub fn check_session_alive(s: &ShellSession) -> Result<(), &'static str> {
  if !s.is_alive.load(Ordering::Acquire) {
    if let Ok(state) = s.state.lock() {
      return Err(match *state {
        SessionState::Ended => s.messages.ended,
        SessionState::Corrupted(ref reason) => {
          shlog_error!("Session corrupted: {}", reason);
          s.messages.corrupted
        }
        SessionState::Running => s.messages.unexpected,
      });
    } else {
      return Err(s.messages.poisoned);
    }
  }
  Ok(())
}

/// Write to the transport, marking the session ended if the connection drops.
pub fn session_write(s: &ShellSession, data: &[u8]) -> Result<(), &'static str> {
  match s.transport.write_all(data) {
    Ok(()) => Ok(()),
    Err(TransportError::ConnectionLost) => {
      s.is_alive.store(false, Ordering::Release);
      if let Ok(mut st) = s.state.lock() {
        *st = SessionState::Ended;
      }
      shlog_trace!("Connection lost during write");
      Err(s.messages.ended)
    }
    Err(TransportError::Other(e)) => Err(e),
  }
}

fn now_marker() -> String {
  format!(
    "__SHARDS_PROMPT_{:x}__",
    std::time::SystemTime::now()
      .duration_since(std::time::UNIX_EPOCH)
      .unwrap_or_default()
      .as_nanos()
      & 0xFFFFFFFF
  )
}

// ============================================================================
// Bootstrap helpers (used by each module's Create/Connect shard)
// ============================================================================

/// Poll the shared buffer until a prompt appears (or retries are exhausted).
pub fn wait_for_initial_prompt(buffer: &Arc<Mutex<Vec<u8>>>) -> Result<(), &'static str> {
  std::thread::sleep(Duration::from_millis(INITIAL_PROMPT_WAIT_MS));

  for _ in 0..INITIAL_PROMPT_MAX_RETRIES {
    {
      let buf = buffer.lock().map_err(|_| "Buffer lock poisoned")?;
      if !buf.is_empty() {
        let output_str = String::from_utf8_lossy(&buf);
        if is_prompt(&output_str) {
          shlog_trace!("Initial prompt detected");
          break;
        }
      }
    }
    std::thread::sleep(Duration::from_millis(ITERATION_SLEEP_MS));
  }

  shlog_trace!("Shell ready");
  Ok(())
}

/// Try to install a `PS1` sentinel marker for reliable prompt detection. On
/// success the shared buffer is cleared and the byte counter reset so setup
/// output never leaks into command results. Returns the marker if confirmed.
pub fn setup_prompt_marker(
  transport: &Arc<dyn ShellTransport>,
  buffer: &Arc<Mutex<Vec<u8>>>,
  total_bytes: &Arc<AtomicUsize>,
) -> Option<String> {
  let marker = now_marker();
  let ps1_cmd = format!("export PS1='{}'\n", marker);

  if transport.write_all(ps1_cmd.as_bytes()).is_err() {
    return None;
  }

  // Wait for the marker to appear.
  std::thread::sleep(Duration::from_millis(300));

  let marker_found = if let Ok(buf) = buffer.lock() {
    String::from_utf8_lossy(&buf).contains(&marker)
  } else {
    false
  };

  // Clear buffer + reset counter regardless — we don't want setup output in
  // command results. Reset the counter while still holding the buffer lock so
  // the reader thread can't append bytes (and bump the counter) in the gap
  // between the clear and the reset, which would leave the counter out of sync
  // with the buffer (matches the ordering in execute / capture_exit_code).
  if let Ok(mut buf) = buffer.lock() {
    buf.clear();
    total_bytes.store(0, Ordering::Release);
  }

  if marker_found {
    shlog_trace!("Sentinel prompt marker set: {}", marker);
    Some(marker)
  } else {
    shlog_trace!("Sentinel prompt marker not detected, falling back to pattern matching");
    None
  }
}

// ============================================================================
// Exit code capture
// ============================================================================

/// Capture the previous command's exit code by sending `echo $?`.
fn capture_exit_code(s: &ShellSession, marker: Option<&str>) -> Option<i64> {
  {
    let mut buf = s.output_buffer.lock().ok()?;
    buf.clear();
    s.total_bytes_written.store(0, Ordering::Release);
    s.read_position.store(0, Ordering::Release);
  }

  if session_write(s, b"echo $?\n").is_err() {
    return None;
  }

  // Wait for response.
  std::thread::sleep(Duration::from_millis(200));

  for _ in 0..10 {
    let snapshot = {
      let buf = s.output_buffer.lock().ok()?;
      buf.clone()
    };

    let output = String::from_utf8_lossy(&snapshot);
    if is_prompt_or_marker(&output, marker) {
      let stripped = strip_ansi_escapes::strip(output.as_bytes());
      let cleaned = String::from_utf8_lossy(&stripped);
      for line in cleaned.lines() {
        let trimmed = line.trim();
        // Skip the echo command itself and the prompt.
        if trimmed == "echo $?" || trimmed.is_empty() {
          continue;
        }
        if is_prompt_or_marker(trimmed, marker) {
          continue;
        }
        if let Ok(code) = trimmed.parse::<i64>() {
          return Some(code);
        }
      }
      return None;
    }

    std::thread::sleep(Duration::from_millis(ITERATION_SLEEP_MS));
  }

  None
}

// ============================================================================
// Generic shard operations
// ============================================================================

/// Execute a command in the session and collect its output.
///
/// `interact_hint` is the module-specific message returned when the command
/// appears to be waiting for input (e.g. "...Use SSH.SendInput to interact.").
pub fn execute(
  s: &ShellSession,
  cmd: &str,
  timeout_secs: i64,
  should_clean: bool,
  max_output_bytes: i64,
  interact_hint: &'static str,
) -> Result<ClonedVar, &'static str> {
  check_session_alive(s)?;

  let max_output_bytes = if max_output_bytes <= 0 {
    usize::MAX
  } else {
    max_output_bytes as usize
  };
  let timeout_secs = if timeout_secs <= 0 { 30 } else { timeout_secs };
  let max_iterations = (timeout_secs as usize) * 10;

  let marker = s.prompt_marker.as_deref();

  // If a previous interactive command is still pending, interrupt it.
  {
    let mut pending = match s.pending_interactive.lock() {
      Ok(guard) => guard,
      Err(_) => {
        mark_session_corrupted(s, "interactive lock poisoned in Execute");
        return Err("Interactive state lock poisoned");
      }
    };
    if pending.is_some() {
      shlog_trace!("New command received while interactive command was pending, sending Ctrl+C");
      let _ = session_write(s, &[3]);
      std::thread::sleep(Duration::from_millis(300));
      *pending = None;
    }
  }

  // Clear shared buffer and reset monotonic counter before sending command.
  {
    let mut shared_buffer = match s.output_buffer.lock() {
      Ok(guard) => guard,
      Err(_) => {
        mark_session_corrupted(s, "buffer lock poisoned before command");
        return Err("Buffer lock poisoned");
      }
    };
    shared_buffer.clear();
    s.total_bytes_written.store(0, Ordering::Release);
    s.read_position.store(0, Ordering::Release);
    shlog_trace!("Cleared shared buffer and reset counters before command");
  }

  // Send command.
  let cmd_with_newline = format!("{}\n", cmd);
  session_write(s, cmd_with_newline.as_bytes())?;

  // Read output using the monotonic byte counter (immune to truncation).
  let mut output_buffer = Vec::new();
  let mut last_total_bytes = 0usize;
  let mut no_data_count = 0;
  let mut prompt_detected = false;
  let mut was_truncated = false;
  let mut seen_command_echo = false;

  // Scale silence threshold with timeout: at least 1/3 of max_iterations, but
  // no less than the default 3 seconds. This prevents false
  // requires_interaction on slow commands (network I/O, compilation, etc.)
  let silence_threshold = std::cmp::max(INTERACTIVE_DETECTION_ITERATIONS, max_iterations / 3);

  shlog_trace!(
    "Starting to read command output from shared buffer (silence_threshold={})",
    silence_threshold
  );
  std::thread::sleep(Duration::from_millis(COMMAND_OUTPUT_WAIT_MS));

  for iteration in 0..max_iterations {
    let current_total = s.total_bytes_written.load(Ordering::Acquire);
    let has_new_data = current_total > last_total_bytes;

    if has_new_data {
      // Mark that the command has started producing output as soon as any
      // bytes arrive, so a small MaxOutputBytes or fast/large output can't
      // cause us to miss the signal.
      if !seen_command_echo {
        seen_command_echo = true;
        shlog_trace!("Command has started producing output");
      }

      let snapshot = {
        let shared_buffer = s.output_buffer.lock().map_err(|_| "Buffer lock poisoned")?;
        shared_buffer.clone()
      };

      output_buffer = snapshot;
      last_total_bytes = current_total;

      // Detect prompts/interaction on the FULL buffer; the output retention
      // cap is applied once after the loop. Truncating here would let a cap
      // smaller than the prompt marker cut the marker off the tail and break
      // completion detection.
      let output_str = String::from_utf8_lossy(&output_buffer);

      shlog_trace!(
        "Read data (iteration {}), total_bytes: {}, buffer size: {}, last line: {:?}",
        iteration,
        current_total,
        output_buffer.len(),
        output_str.lines().last()
      );

      if is_prompt_or_marker(&output_str, marker) {
        prompt_detected = true;
        shlog_trace!("Prompt detected in output, command completed");
        break;
      }

      if is_interactive_prompt(&output_str) {
        shlog_trace!(
          "Interactive prompt pattern detected early: {:?}",
          output_str.lines().last()
        );
        break;
      }

      no_data_count = 0;
    } else {
      no_data_count += 1;
      shlog_trace!(
        "No data (iteration {}), no_data_count: {}, buffer_empty: {}, seen_echo: {}",
        iteration,
        no_data_count,
        output_buffer.is_empty(),
        seen_command_echo
      );

      // Fallback interactive detection after the silence threshold expires.
      // Only trigger if the command has started producing output — before
      // that, the command hasn't had a chance to run yet.
      if no_data_count >= silence_threshold && seen_command_echo {
        let output_str = String::from_utf8_lossy(&output_buffer);

        if is_prompt_or_marker(&output_str, marker) {
          prompt_detected = true;
          shlog_trace!("Prompt detected on final check");
          break;
        }

        if is_interactive_prompt(&output_str) {
          shlog_trace!(
            "Interactive prompt pattern detected: {:?}",
            output_str.lines().last()
          );
          break;
        }

        shlog_trace!(
          "Command appears to be interactive (no new data for {}ms, no prompt detected)",
          silence_threshold as u64 * ITERATION_SLEEP_MS
        );
        break;
      }
    }
    std::thread::sleep(Duration::from_millis(ITERATION_SLEEP_MS));
  }

  // Apply the output retention cap now that detection ran on the full buffer.
  if truncate_to_tail(&mut output_buffer, max_output_bytes) {
    was_truncated = true;
  }

  // Build result table.
  let mut result_table = AutoTableVar::new();
  let output_str = String::from_utf8_lossy(&output_buffer).to_string();

  if prompt_detected {
    let exit_code = capture_exit_code(s, marker);

    let final_output = if should_clean {
      clean_output_with_marker(&output_str, marker)
    } else {
      output_str
    };

    result_table
      .0
      .insert_fast_static("status", &Var::ephemeral_string("completed"));
    result_table
      .0
      .insert_fast_static("output", &Var::ephemeral_string(&final_output));
    if let Some(code) = exit_code {
      result_table
        .0
        .insert_fast_static("exit_code", &Var::from(code));
    }
    if was_truncated {
      result_table.0.insert_fast_static("truncated", &true.into());
    }
  } else {
    // Command is interactive (waiting for input).
    {
      let mut pending = s
        .pending_interactive
        .lock()
        .map_err(|_| "Interactive state lock poisoned")?;
      *pending = Some(InteractiveState {
        original_cmd: cmd.to_string(),
      });
    }

    let partial_output = if should_clean {
      clean_output_with_marker(&output_str, marker)
    } else {
      output_str
    };

    result_table
      .0
      .insert_fast_static("status", &Var::ephemeral_string("requires_interaction"));
    result_table
      .0
      .insert_fast_static("output", &Var::ephemeral_string(&partial_output));
    result_table
      .0
      .insert_fast_static("message", &Var::ephemeral_string(interact_hint));
    if was_truncated {
      result_table.0.insert_fast_static("truncated", &true.into());
    }
  }

  // Sync read position so a subsequent Read doesn't pick up stale data from
  // command execution or exit code capture.
  let final_total = s.total_bytes_written.load(Ordering::Acquire);
  s.read_position.store(final_total, Ordering::Release);

  Ok(result_table.to_cloned())
}

/// Send input to a pending interactive command.
///
/// `continue_hint` is the module-specific message returned when the command is
/// still running (e.g. "...Use SSH.SendInput to continue.").
pub fn send_input(
  s: &ShellSession,
  input_str: &str,
  timeout_secs: i64,
  max_output_bytes: i64,
  continue_hint: &'static str,
) -> Result<ClonedVar, &'static str> {
  check_session_alive(s)?;

  let max_output_bytes = if max_output_bytes <= 0 {
    usize::MAX
  } else {
    max_output_bytes as usize
  };
  let timeout_secs = if timeout_secs <= 0 { 10 } else { timeout_secs };
  let max_iterations = (timeout_secs as usize) * 10;

  let marker = s.prompt_marker.as_deref();

  // There must be a pending interactive command.
  {
    let pending = match s.pending_interactive.lock() {
      Ok(guard) => guard,
      Err(_) => {
        mark_session_corrupted(s, "interactive lock poisoned in SendInput");
        return Err("Interactive state lock poisoned");
      }
    };
    if pending.is_none() {
      return Err("No interactive command is pending");
    }
  }

  // Record monotonic position before sending input.
  let start_total_bytes = s.total_bytes_written.load(Ordering::Acquire);

  // Send input (if not empty).
  if !input_str.is_empty() {
    let input_with_newline = format!("{}\n", input_str);
    shlog_trace!(
      "SendInput: Writing {} bytes: {:?}",
      input_with_newline.len(),
      input_with_newline
    );
    session_write(s, input_with_newline.as_bytes())?;
    shlog_trace!("SendInput: Write succeeded");
  }

  // Wait for output.
  std::thread::sleep(Duration::from_millis(SENDINPUT_INITIAL_WAIT_MS));
  shlog_trace!(
    "SendInput: Starting to read output after input (start_total_bytes={})",
    start_total_bytes
  );

  let mut output_buffer = Vec::new();
  let mut last_total_bytes = start_total_bytes;
  let mut was_truncated = false;
  let mut no_data_count = 0;
  let mut prompt_detected = false;

  for iteration in 0..max_iterations {
    let current_total = s.total_bytes_written.load(Ordering::Acquire);
    let has_new_data = current_total > last_total_bytes;

    if has_new_data {
      let snapshot = {
        let shared_buffer = s.output_buffer.lock().map_err(|_| "Buffer lock poisoned")?;
        shared_buffer.clone()
      };

      output_buffer = snapshot;
      last_total_bytes = current_total;

      // Detect on the FULL buffer; the retention cap is applied once after the
      // loop so a tiny MaxOutputBytes can't truncate the prompt marker away.
      let output_str = String::from_utf8_lossy(&output_buffer);
      shlog_trace!(
        "SendInput: Read data (iteration {}), total_bytes: {}, buffer size: {}",
        iteration,
        current_total,
        output_buffer.len()
      );

      no_data_count = 0;

      if is_prompt_or_marker(&output_str, marker) {
        shlog_trace!("SendInput: Prompt detected in new output, exiting early");
        prompt_detected = true;
        break;
      }
    } else {
      no_data_count += 1;
      shlog_trace!(
        "SendInput: No new data (iteration {}), no_data_count: {}",
        iteration,
        no_data_count
      );

      if no_data_count >= SENDINPUT_NO_DATA_THRESHOLD {
        let output_str = String::from_utf8_lossy(&output_buffer);
        if is_prompt_or_marker(&output_str, marker) {
          shlog_trace!("SendInput: Prompt detected after silence, exiting");
          prompt_detected = true;
          break;
        }
      }
    }
    std::thread::sleep(Duration::from_millis(ITERATION_SLEEP_MS));
  }

  // Final prompt check.
  if !prompt_detected {
    let output_str = String::from_utf8_lossy(&output_buffer);
    prompt_detected = is_prompt_or_marker(&output_str, marker);
  }

  // Apply the output retention cap now that detection ran on the full buffer.
  if truncate_to_tail(&mut output_buffer, max_output_bytes) {
    was_truncated = true;
  }

  // Clean output.
  let output_str = String::from_utf8_lossy(&output_buffer).to_string();
  let stripped_bytes = strip_ansi_escapes::strip(&output_str);
  let cleaned = String::from_utf8_lossy(&stripped_bytes);
  let mut lines: Vec<&str> = cleaned.lines().collect();
  if prompt_detected && !lines.is_empty() {
    lines.pop(); // Remove prompt line.
  }
  let final_output = lines.join("\n");

  let mut result_table = AutoTableVar::new();

  if prompt_detected {
    {
      let mut pending = s
        .pending_interactive
        .lock()
        .map_err(|_| "Interactive state lock poisoned")?;
      *pending = None;
    }
    shlog_trace!("Interactive session completed");
    result_table
      .0
      .insert_fast_static("status", &Var::ephemeral_string("completed"));
    result_table
      .0
      .insert_fast_static("output", &Var::ephemeral_string(&final_output));
    if was_truncated {
      result_table.0.insert_fast_static("truncated", &true.into());
    }
  } else {
    result_table
      .0
      .insert_fast_static("status", &Var::ephemeral_string("pending_output"));
    result_table
      .0
      .insert_fast_static("output", &Var::ephemeral_string(&final_output));
    result_table
      .0
      .insert_fast_static("message", &Var::ephemeral_string(continue_hint));
    if was_truncated {
      result_table.0.insert_fast_static("truncated", &true.into());
    }
  }

  // Sync read position.
  let final_total = s.total_bytes_written.load(Ordering::Acquire);
  s.read_position.store(final_total, Ordering::Release);

  Ok(result_table.to_cloned())
}

/// Raw, non-blocking read of accumulated output. Cooperatively suspends the wire
/// (via `context`) while waiting for data to settle when a timeout is given.
pub fn read_raw(
  s: &ShellSession,
  context: &Context,
  max_bytes: i64,
  strip: bool,
  timeout_ms: i64,
) -> Result<String, &'static str> {
  check_session_alive(s)?;

  let max_bytes = if max_bytes <= 0 {
    65536usize
  } else {
    max_bytes as usize
  };
  let timeout_ms = if timeout_ms < 0 {
    0u64
  } else {
    timeout_ms as u64
  };

  // Read cursor lives on the session so multiple Read shard instances share it.
  let read_pos = &s.read_position;

  let deadline = if timeout_ms > 0 {
    Some(std::time::Instant::now() + Duration::from_millis(timeout_ms))
  } else {
    None
  };

  let mut result_data = Vec::new();

  // Accumulate data until timeout expires or data settles. Settle time scales
  // with timeout: 20% of requested timeout, clamped [100ms, 2000ms]. TUI apps
  // (Ink/React) render in bursts with gaps — a longer settle avoids returning
  // between render bursts with only partial output.
  let settle_ms = if timeout_ms > 0 {
    (timeout_ms / 5).max(100).min(2000)
  } else {
    100
  };
  let mut last_data_time: Option<std::time::Instant> = None;
  let mut prev_total = read_pos.load(Ordering::Acquire);

  loop {
    let current_total = s.total_bytes_written.load(Ordering::Acquire);

    if current_total > prev_total {
      // New data arrived since last poll — reset settle timer.
      last_data_time = Some(std::time::Instant::now());
      prev_total = current_total;
    }

    if let Some(dl) = deadline {
      // Hard timeout.
      if std::time::Instant::now() >= dl {
        break;
      }
      // Early exit: data arrived then went quiet for settle_ms.
      if let Some(ldt) = last_data_time {
        if std::time::Instant::now().duration_since(ldt) >= Duration::from_millis(settle_ms) {
          break;
        }
      }
      // Yield to the scheduler instead of blocking the thread.
      shards::core::suspend(context, 0.01);
    } else {
      // No timeout = immediate mode, return whatever is available now.
      break;
    }
  }

  // Final snapshot of accumulated data.
  let final_total = s.total_bytes_written.load(Ordering::Acquire);
  let final_consumed = read_pos.load(Ordering::Acquire);
  if final_total > final_consumed {
    let snapshot = {
      let buf = s.output_buffer.lock().map_err(|_| "Buffer lock poisoned")?;
      buf.clone()
    };

    let new_byte_count = final_total - final_consumed;
    let available = snapshot.len().min(new_byte_count).min(max_bytes);
    result_data = snapshot[snapshot.len().saturating_sub(available)..].to_vec();
    read_pos.store(final_total, Ordering::Release);
  }

  let output_str = if strip {
    let term_rows = s.term_rows.load(Ordering::Acquire);
    let term_cols = s.term_cols.load(Ordering::Acquire);
    render_through_virtual_terminal(&result_data, term_rows, term_cols)
  } else {
    String::from_utf8_lossy(&result_data).to_string()
  };

  Ok(output_str)
}

/// Raw byte write to the session, interpreting escape sequences.
pub fn write_raw(s: &ShellSession, input_str: &str, append_nl: bool) -> Result<(), &'static str> {
  check_session_alive(s)?;

  let bytes = interpret_escape_sequences(input_str);
  session_write(s, &bytes)?;
  if append_nl {
    session_write(s, b"\r")?;
  }
  Ok(())
}

/// Resize the session's PTY and remember the new dimensions.
pub fn resize(s: &ShellSession, rows: i64, cols: i64) -> Result<(), &'static str> {
  check_session_alive(s)?;

  let rows = rows.max(1).min(u16::MAX as i64) as u16;
  let cols = cols.max(1).min(u16::MAX as i64) as u16;

  s.transport.resize(rows, cols)?;
  s.term_rows.store(rows, Ordering::Release);
  s.term_cols.store(cols, Ordering::Release);
  shlog_trace!("PTY resized to {}x{}", cols, rows);
  Ok(())
}

/// Watch the session's output stream until `pattern` (a regular expression)
/// matches, returning the matched text. Cooperatively suspends the wire (via
/// `context`) between polls so the reader thread keeps filling the buffer and
/// the wire stays responsive.
///
/// Matching is performed against output accumulated from the shared read cursor
/// forward; on a match the cursor is advanced past everything observed so far,
/// so a subsequent `Read`/`WaitFor` continues after the matched region. When
/// `strip` is set, ANSI escape sequences are removed before matching (and from
/// the returned text). `timeout_ms` of 0 means wait indefinitely (until the
/// session ends). Returns an error on timeout or if the session ends first.
pub fn wait_for(
  s: &ShellSession,
  context: &Context,
  pattern: &str,
  timeout_ms: i64,
  strip: bool,
) -> Result<String, &'static str> {
  check_session_alive(s)?;

  let re = regex::Regex::new(pattern).map_err(|_| "Invalid regex pattern")?;

  let timeout_ms = if timeout_ms < 0 {
    0u64
  } else {
    timeout_ms as u64
  };
  let deadline = if timeout_ms > 0 {
    Some(std::time::Instant::now() + Duration::from_millis(timeout_ms))
  } else {
    None
  };

  // Shared read cursor, so this composes with Read/Execute on the same session.
  let read_pos = &s.read_position;

  loop {
    let current_total = s.total_bytes_written.load(Ordering::Acquire);
    let consumed = read_pos.load(Ordering::Acquire);

    if current_total > consumed {
      let snapshot = {
        let buf = s.output_buffer.lock().map_err(|_| "Buffer lock poisoned")?;
        buf.clone()
      };

      // The monotonic counter can outrun the (truncated) buffer; clamp to what
      // is actually retained.
      let new_count = current_total - consumed;
      let available = snapshot.len().min(new_count);
      let new_slice = &snapshot[snapshot.len() - available..];

      let text = if strip {
        let stripped = strip_ansi_escapes::strip(new_slice);
        String::from_utf8_lossy(&stripped).to_string()
      } else {
        String::from_utf8_lossy(new_slice).to_string()
      };

      if let Some(m) = re.find(&text) {
        let matched = m.as_str().to_string();
        // Consume everything observed so far.
        read_pos.store(current_total, Ordering::Release);
        shlog_trace!("WaitFor: pattern matched: {:?}", matched);
        return Ok(matched);
      }
    }

    // Stop if the session ended (process exited / connection lost). Done after
    // the match attempt so the final output is still considered.
    if !s.is_alive.load(Ordering::Acquire) {
      return Err(s.messages.ended);
    }

    if let Some(dl) = deadline {
      if std::time::Instant::now() >= dl {
        return Err("Timed out waiting for pattern");
      }
    }

    // Yield to the scheduler instead of blocking the thread.
    shards::core::suspend(context, 0.05);
  }
}
