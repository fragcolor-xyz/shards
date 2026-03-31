/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2025 Fragcolor Pte. Ltd. */

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
use shards::types::AutoTableVar;
use shards::types::ClonedVar;
use shards::types::Context;
use shards::types::ExposedTypes;
use shards::types::InstanceData;
use shards::types::ParamVar;
use shards::types::Type;
use shards::types::Types;
use shards::types::Var;
use shards::types::FRAG_CC;
use shards::types::STRING_TYPES;
use shards::types::NONE_TYPES;
use shards::types::BOOL_TYPES;
use ssh2::Session;
use std::io::prelude::*;
use std::net::{TcpStream, ToSocketAddrs};
use std::path::Path;
use std::sync::{Arc, Mutex};
use std::sync::atomic::{AtomicBool, AtomicU16, AtomicUsize, Ordering};
use std::time::Duration;

// Configuration constants
const INITIAL_PROMPT_WAIT_MS: u64 = 500;
const INITIAL_PROMPT_MAX_RETRIES: usize = 20;
const COMMAND_OUTPUT_WAIT_MS: u64 = 200;
const ITERATION_SLEEP_MS: u64 = 100;
const INTERACTIVE_DETECTION_ITERATIONS: usize = 30; // 30 * 100ms = 3 seconds
const SENDINPUT_INITIAL_WAIT_MS: u64 = 500;
const SENDINPUT_NO_DATA_THRESHOLD: usize = 20; // 20 iterations = 2 seconds
const MAX_BUFFER_BYTES: usize = 65536; // 64KB default
const BUFFER_TRUNCATE_KEEP_RATIO: usize = 93; // Keep 93% on truncation
const READER_POLL_MS: u64 = 50; // How often the reader thread polls the channel

// SSH Shell object wrapper
mod ssh_shell {
    use super::*;

    #[derive(Debug, Clone, PartialEq)]
    pub enum SessionState {
        Running,
        Disconnected,
        Corrupted(String),
    }

    pub struct SSHShell {
        pub session: Arc<Mutex<Session>>,
        pub channel: Arc<Mutex<ssh2::Channel>>,
        pub pending_interactive: Arc<Mutex<Option<InteractiveState>>>,
        pub is_connected: Arc<AtomicBool>,
        pub state: Arc<Mutex<SessionState>>,
        pub output_buffer: Arc<Mutex<Vec<u8>>>,
        pub total_bytes_written: Arc<AtomicUsize>,
        pub read_position: Arc<AtomicUsize>,
        pub reader_thread: Arc<Mutex<Option<std::thread::JoinHandle<()>>>>,
        pub prompt_marker: Option<String>,
        pub term_rows: Arc<AtomicU16>,
        pub term_cols: Arc<AtomicU16>,
    }

    #[derive(Clone)]
    pub struct InteractiveState {
        #[allow(dead_code)]
        pub original_cmd: String,
    }

    impl Drop for SSHShell {
        fn drop(&mut self) {
            shlog_trace!("Dropping SSHShell, cleaning up connection");

            // Step 1: Mark as disconnected (stops reader thread loop)
            self.is_connected.store(false, Ordering::Release);

            // Step 2: Close channel gracefully
            if let Ok(mut channel) = self.channel.lock() {
                let _ = channel.send_eof();
                let _ = channel.wait_eof();
                let _ = channel.close();
                let _ = channel.wait_close();
            }

            // Step 3: Disconnect session
            if let Ok(session) = self.session.lock() {
                let _ = session.disconnect(None, "Session closed", None);
            }

            // Step 4: Wait for reader thread with timeout
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
                        shlog_error!("Reader thread did not finish within timeout, leaving it detached (thread leak)");
                    }
                }
            }

            shlog_trace!("SSHShell cleanup complete");
        }
    }

    ref_counted_object_type_impl!(SSHShell);
}

use ssh_shell::*;

lazy_static! {
    static ref SSH_SHELL_TYPE: Type = Type::object(FRAG_CC, fourCharacterCode(*b"sshs"));
    static ref SSH_SHELL_TYPE_VEC: Vec<Type> = vec![*SSH_SHELL_TYPE];
    static ref SSH_SHELL_VAR_TYPE: Type = Type::context_variable(&SSH_SHELL_TYPE_VEC);
    static ref EXECUTE_OUTPUT_TYPES: Vec<Type> = vec![common_type::string_table];
}

// Helper: detect shell prompts with optional sentinel marker
fn is_prompt_or_marker(text: &str, marker: Option<&str>) -> bool {
    let stripped_bytes = strip_ansi_escapes::strip(text.as_bytes());
    let cleaned = String::from_utf8_lossy(&stripped_bytes);

    let lines: Vec<&str> = cleaned.lines().collect();
    if let Some(last) = lines.last() {
        let trimmed = last.trim();

        // Check sentinel marker first (most reliable)
        if let Some(m) = marker {
            // When sentinel marker is set, ONLY trust the sentinel.
            // Generic prompt patterns ($ # > %) cause false positives with
            // heredoc continuation prompts and command output that happens
            // to end with these characters.
            return trimmed.contains(m);
        }

        // Exclude interactive program prompts like >>> (Python), >> (continuation)
        if trimmed.ends_with(">>>") || trimmed.ends_with(">>") {
            return false;
        }

        // Check for common SHELL prompt patterns (fallback when no sentinel)
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

fn is_prompt(text: &str) -> bool {
    is_prompt_or_marker(text, None)
}

// Helper: detect common interactive prompts (password, confirmation, etc.)
fn is_interactive_prompt(text: &str) -> bool {
    let stripped_bytes = strip_ansi_escapes::strip(text.as_bytes());
    let cleaned = String::from_utf8_lossy(&stripped_bytes);

    let lines: Vec<&str> = cleaned.lines().collect();
    if lines.is_empty() {
        return false;
    }

    let check_lines = if lines.len() > 3 { &lines[lines.len()-3..] } else { &lines[..] };

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

// Helper: mark session as corrupted
fn mark_session_corrupted(ssh_shell: &SSHShell, error_context: &str) {
    shlog_error!("Critical error in SSH session ({}), marking as corrupted", error_context);
    ssh_shell.is_connected.store(false, Ordering::Release);
    if let Ok(mut state) = ssh_shell.state.lock() {
        *state = SessionState::Corrupted(error_context.to_string());
    }
}

// Helper: clean output (remove ANSI codes, control chars, and trailing prompts)
fn clean_output_with_marker(output: &str, marker: Option<&str>) -> String {
    let stripped_bytes = strip_ansi_escapes::strip(output);
    let text = String::from_utf8_lossy(&stripped_bytes);

    let no_control: String = text.chars()
        .filter(|c| !c.is_control() || *c == '\n' || *c == '\t')
        .collect();

    let mut lines: Vec<&str> = no_control.lines().collect();

    // Remove trailing prompt if present
    if !lines.is_empty() {
        let last_line = lines.last().unwrap();
        if is_prompt_or_marker(last_line, marker) {
            lines.pop();
        }
    }

    let result = lines.join("\n");
    result.trim().to_string()
}

// Helper: truncate output buffer to keep tail
fn truncate_to_tail(buffer: &mut Vec<u8>, max_bytes: usize) -> bool {
    if buffer.len() <= max_bytes {
        return false;
    }

    let keep_bytes = (max_bytes * BUFFER_TRUNCATE_KEEP_RATIO) / 100;
    let truncate_msg = b"[... output truncated ...]\n";

    let skip = buffer.len() - keep_bytes + truncate_msg.len();
    let tail: Vec<u8> = buffer.drain(skip..).collect();
    buffer.clear();
    buffer.extend_from_slice(truncate_msg);
    buffer.extend_from_slice(&tail);

    true
}

// Helper: check if an IO error indicates connection loss
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

// Helper: check if session is alive, return descriptive error if not
fn check_session_alive(ssh_shell: &SSHShell) -> Result<(), &'static str> {
    if !ssh_shell.is_connected.load(Ordering::Acquire) {
        if let Ok(state) = ssh_shell.state.lock() {
            return Err(match *state {
                SessionState::Disconnected => "SSH connection lost",
                SessionState::Corrupted(ref reason) => {
                    shlog_error!("Session corrupted: {}", reason);
                    "SSH session corrupted due to internal error"
                },
                SessionState::Running => "SSH connection is not alive (unexpected state)",
            });
        } else {
            return Err("SSH session corrupted (state lock poisoned)");
        }
    }
    Ok(())
}

// Helper: extract SSHShell from Var
fn get_session(session_var: &Var) -> Result<&SSHShell, &'static str> {
    let ssh_shell = unsafe {
        Var::from_ref_counted_object::<SSHShell>(session_var, &*SSH_SHELL_TYPE)?
    };
    Ok(unsafe { &*(ssh_shell as *const SSHShell) })
}

// Start the background reader thread for an SSH session.
// The thread polls the channel in non-blocking mode and appends data to the shared buffer.
fn start_reader_thread(
    channel: Arc<Mutex<ssh2::Channel>>,
    session: Arc<Mutex<Session>>,
    buffer: Arc<Mutex<Vec<u8>>>,
    total_bytes: Arc<AtomicUsize>,
    alive: Arc<AtomicBool>,
    state: Arc<Mutex<SessionState>>,
) -> std::thread::JoinHandle<()> {
    std::thread::spawn(move || {
        shlog_trace!("SSH reader thread started");
        let mut buf = [0u8; 4096];

        loop {
            if !alive.load(Ordering::Acquire) {
                shlog_trace!("SSH reader thread exiting (not alive)");
                break;
            }

            // Ensure session is in non-blocking mode before each read
            if let Ok(sess) = session.lock() {
                sess.set_blocking(false);
            }

            let read_result = {
                match channel.lock() {
                    Ok(mut ch) => ch.read(&mut buf),
                    Err(e) => {
                        shlog_trace!("SSH reader thread: channel lock poisoned: {}", e);
                        break;
                    }
                }
            };

            match read_result {
                Ok(n) if n > 0 => {
                    match buffer.lock() {
                        Ok(mut b) => {
                            b.extend_from_slice(&buf[..n]);

                            if b.len() > MAX_BUFFER_BYTES {
                                if truncate_to_tail(&mut b, MAX_BUFFER_BYTES) {
                                    shlog_trace!("SSH reader thread: buffer truncated to {} bytes", b.len());
                                }
                            }

                            total_bytes.fetch_add(n, Ordering::Release);
                            shlog_trace!("SSH reader thread: read {} bytes, buffer now {} bytes", n, b.len());
                        }
                        Err(e) => {
                            shlog_trace!("SSH reader thread: buffer lock poisoned: {}", e);
                            break;
                        }
                    }
                }
                Ok(_) => {
                    // EOF — connection closed
                    alive.store(false, Ordering::Release);
                    if let Ok(mut s) = state.lock() {
                        *s = SessionState::Disconnected;
                    }
                    shlog_trace!("SSH reader thread: EOF detected, connection closed");
                    break;
                }
                Err(e) if e.kind() == std::io::ErrorKind::WouldBlock => {
                    // No data available, sleep briefly and try again
                    std::thread::sleep(Duration::from_millis(READER_POLL_MS));
                    continue;
                }
                Err(e) if is_connection_error(&e) => {
                    alive.store(false, Ordering::Release);
                    if let Ok(mut s) = state.lock() {
                        *s = SessionState::Disconnected;
                    }
                    shlog_trace!("SSH reader thread: connection error: {}", e);
                    break;
                }
                Err(e) => {
                    // Other errors — might be transient, log and sleep
                    shlog_trace!("SSH reader thread: read error (non-fatal): {}", e);
                    std::thread::sleep(Duration::from_millis(READER_POLL_MS));
                }
            }
        }
        shlog_trace!("SSH reader thread finished");
    })
}

// Helper: write to channel, handling connection errors
fn write_to_channel(ssh_shell: &SSHShell, data: &[u8]) -> Result<(), &'static str> {
    let mut channel = match ssh_shell.channel.lock() {
        Ok(guard) => guard,
        Err(_) => {
            mark_session_corrupted(ssh_shell, "channel lock poisoned during write");
            return Err("Channel lock poisoned");
        }
    };

    // Set blocking for writes to ensure they complete
    if let Ok(sess) = ssh_shell.session.lock() {
        sess.set_blocking(true);
    }

    if let Err(e) = channel.write_all(data) {
        // Restore non-blocking
        if let Ok(sess) = ssh_shell.session.lock() {
            sess.set_blocking(false);
        }
        if is_connection_error(&e) {
            ssh_shell.is_connected.store(false, Ordering::Release);
            if let Ok(mut s) = ssh_shell.state.lock() {
                *s = SessionState::Disconnected;
            }
            shlog_trace!("Connection lost during write: {:?}", e);
            return Err("SSH connection lost");
        }
        return Err("Failed to write to SSH channel");
    }

    let _ = channel.flush();

    // Restore non-blocking
    if let Ok(sess) = ssh_shell.session.lock() {
        sess.set_blocking(false);
    }

    Ok(())
}

// Helper: capture exit code by sending `echo $?` after command completes
fn capture_exit_code(ssh_shell: &SSHShell, marker: Option<&str>) -> Option<i64> {
    // Clear buffer, send echo $?, wait for prompt, parse result
    {
        let mut buf = match ssh_shell.output_buffer.lock() {
            Ok(b) => b,
            Err(_) => return None,
        };
        buf.clear();
        ssh_shell.total_bytes_written.store(0, Ordering::Release);
        ssh_shell.read_position.store(0, Ordering::Release);
    }

    if write_to_channel(ssh_shell, b"echo $?\n").is_err() {
        return None;
    }

    // Wait for response
    std::thread::sleep(Duration::from_millis(200));

    for _ in 0..10 {
        let snapshot = {
            let buf = ssh_shell.output_buffer.lock().ok()?;
            buf.clone()
        };

        let output = String::from_utf8_lossy(&snapshot);
        if is_prompt_or_marker(&output, marker) {
            let stripped = strip_ansi_escapes::strip(output.as_bytes());
            let cleaned = String::from_utf8_lossy(&stripped);
            for line in cleaned.lines() {
                let trimmed = line.trim();
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

/// Render raw PTY output through a virtual VT100 terminal to get properly-spaced text.
fn render_through_virtual_terminal(raw_bytes: &[u8], rows: u16, cols: u16) -> String {
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

// Interpret escape sequences in input strings for PTY writing.
fn interpret_escape_sequences(input: &str) -> Vec<u8> {
    let mut result = Vec::with_capacity(input.len());
    let mut chars = input.chars().peekable();

    while let Some(ch) = chars.next() {
        if ch == '\\' {
            match chars.peek() {
                Some('n') => { chars.next(); result.push(0x0A); }
                Some('r') => { chars.next(); result.push(0x0D); }
                Some('t') => { chars.next(); result.push(0x09); }
                Some('\\') => { chars.next(); result.push(b'\\'); }
                Some('x') => {
                    chars.next();
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
                _ => { result.push(b'\\'); }
            }
        } else if ch == '\n' {
            // Bare LF → CR: terminal Enter sends CR to PTY master
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
// SSH.Connect Shard
// ============================================================================

#[derive(shards::shard)]
#[shard_info("SSH.Connect", "Connect to an SSH server and create a persistent shell session")]
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
        let rows: u16 = if rows_val.is_none() { 24 } else {
            let v: i64 = rows_val.as_ref().try_into().unwrap_or(24);
            v.max(1).min(u16::MAX as i64) as u16
        };
        let cols: u16 = if cols_val.is_none() { 80 } else {
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

        let tcp = TcpStream::connect_timeout(
            socket_addr,
            Duration::from_secs(timeout_secs as u64),
        )
        .map_err(|_| "Connection failed")?;

        tcp.set_read_timeout(Some(Duration::from_secs(timeout_secs as u64)))
            .map_err(|_| "Failed to set timeout")?;

        let mut sess = Session::new().map_err(|_| "Failed to create SSH session")?;
        sess.set_tcp_stream(tcp);
        sess.handshake().map_err(|_| "SSH handshake failed")?;

        // Authenticate
        if !key_path_var.is_none() {
            let key_path: &str = key_path_var.as_ref().try_into()?;
            let key_path_expanded = shellexpand::tilde(key_path).to_string();
            sess.userauth_pubkey_file(user, None, Path::new(&key_path_expanded), None)
                .map_err(|_| "Public key authentication failed")?;
        } else if !password_var.is_none() {
            let password: &str = password_var.as_ref().try_into()?;
            sess.userauth_password(user, password)
                .map_err(|_| "Password authentication failed")?;
        } else {
            return Err("Either KeyPath or Password must be provided");
        }

        if !sess.authenticated() {
            return Err("Authentication failed");
        }

        // Open channel and request PTY with size
        let mut channel = sess.channel_session().map_err(|_| "Failed to open channel")?;
        channel
            .request_pty("xterm-256color", None, Some((cols as u32, rows as u32, 0, 0)))
            .map_err(|_| "Failed to request PTY")?;
        channel.shell().map_err(|_| "Failed to start shell")?;

        // Set non-blocking mode for reads
        sess.set_blocking(false);

        // Create shared state
        let channel_arc = Arc::new(Mutex::new(channel));
        let session_arc = Arc::new(Mutex::new(sess));
        let output_buffer = Arc::new(Mutex::new(Vec::new()));
        let total_bytes_written = Arc::new(AtomicUsize::new(0));
        let is_connected = Arc::new(AtomicBool::new(true));
        let state = Arc::new(Mutex::new(SessionState::Running));

        // Start reader thread BEFORE waiting for initial prompt
        let reader_thread = start_reader_thread(
            Arc::clone(&channel_arc),
            Arc::clone(&session_arc),
            Arc::clone(&output_buffer),
            Arc::clone(&total_bytes_written),
            Arc::clone(&is_connected),
            Arc::clone(&state),
        );

        // Wait for initial prompt by polling the shared buffer
        std::thread::sleep(Duration::from_millis(INITIAL_PROMPT_WAIT_MS));

        for _ in 0..INITIAL_PROMPT_MAX_RETRIES {
            {
                let buf = output_buffer.lock().map_err(|_| "Buffer lock poisoned")?;
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

        shlog_trace!("Shell ready, attempting to switch to bash for consistent behavior");

        // Helper closure to write to channel (needs blocking mode for reliable writes)
        let write_cmd = |channel_arc: &Arc<Mutex<ssh2::Channel>>, session_arc: &Arc<Mutex<Session>>, data: &[u8]| -> Result<(), &'static str> {
            if let Ok(sess) = session_arc.lock() {
                sess.set_blocking(true);
            }
            let result = {
                let mut ch = channel_arc.lock().map_err(|_| "Channel lock poisoned")?;
                ch.write_all(data).map_err(|_| "Write failed")?;
                ch.flush().map_err(|_| "Flush failed")
            };
            if let Ok(sess) = session_arc.lock() {
                sess.set_blocking(false);
            }
            result
        };

        // Try to switch to bash
        let _ = write_cmd(&channel_arc, &session_arc, b"command -v bash >/dev/null 2>&1 && exec bash --login\n");

        // Wait for bash to start and show prompt
        std::thread::sleep(Duration::from_millis(500));

        let mut bash_switched = false;
        for _ in 0..10 {
            let buf = output_buffer.lock().map_err(|_| "Buffer lock poisoned")?;
            let output_str = String::from_utf8_lossy(&buf);
            if is_prompt(&output_str) {
                bash_switched = !output_str.contains("command not found") &&
                               !output_str.contains("not found");
                break;
            }
            drop(buf);
            std::thread::sleep(Duration::from_millis(ITERATION_SLEEP_MS));
        }

        if bash_switched {
            shlog_trace!("Switched to bash");
        } else {
            shlog_trace!("Bash not available or switch failed, continuing with current shell");
        }

        // Try to set PS1 sentinel marker for reliable prompt detection
        let marker = format!("__SHARDS_PROMPT_{:x}__", std::time::SystemTime::now()
            .duration_since(std::time::UNIX_EPOCH)
            .unwrap_or_default()
            .as_nanos() & 0xFFFFFFFF);

        let prompt_marker = {
            let ps1_cmd = format!("export PS1='{}'\n", marker);
            let set_ok = write_cmd(&channel_arc, &session_arc, ps1_cmd.as_bytes()).is_ok();

            if set_ok {
                // Wait for the marker to appear
                std::thread::sleep(Duration::from_millis(300));

                let marker_found = if let Ok(buf) = output_buffer.lock() {
                    let s = String::from_utf8_lossy(&buf);
                    s.contains(&marker)
                } else {
                    false
                };

                if marker_found {
                    shlog_trace!("Sentinel prompt marker set: {}", marker);
                    // Clear buffer after marker setup
                    if let Ok(mut buf) = output_buffer.lock() {
                        buf.clear();
                    }
                    total_bytes_written.store(0, Ordering::Release);
                    Some(marker)
                } else {
                    shlog_trace!("Sentinel prompt marker not detected, falling back to pattern matching");
                    if let Ok(mut buf) = output_buffer.lock() {
                        buf.clear();
                    }
                    total_bytes_written.store(0, Ordering::Release);
                    None
                }
            } else {
                None
            }
        };

        shlog_trace!("SSH connection established");

        let ssh_shell = SSHShell {
            session: session_arc,
            channel: channel_arc,
            pending_interactive: Arc::new(Mutex::new(None)),
            is_connected,
            state,
            output_buffer,
            total_bytes_written,
            read_position: Arc::new(AtomicUsize::new(0)),
            reader_thread: Arc::new(Mutex::new(Some(reader_thread))),
            prompt_marker,
            term_rows: Arc::new(AtomicU16::new(rows)),
            term_cols: Arc::new(AtomicU16::new(cols)),
        };

        let shell_var = Var::new_ref_counted(ssh_shell, &*SSH_SHELL_TYPE);
        self.output = shell_var.into();
        Ok(self.output.0)
    }
}

// ============================================================================
// SSH.Execute Shard
// ============================================================================

#[derive(shards::shard)]
#[shard_info(
    "SSH.Execute",
    "Execute a command in the persistent SSH shell session"
)]
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
        &EXECUTE_OUTPUT_TYPES
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
        let max_output_bytes = if max_output_bytes <= 0 { usize::MAX } else { max_output_bytes as usize };

        let timeout_secs: i64 = self.timeout_secs.get().as_ref().try_into()?;
        let timeout_secs = if timeout_secs <= 0 { 30 } else { timeout_secs };
        let max_iterations = (timeout_secs as usize) * 10;

        let ssh_shell = get_session(&session_var)?;
        check_session_alive(ssh_shell)?;

        let marker = ssh_shell.prompt_marker.as_deref();

        // Check if there's a pending interactive command
        {
            let mut pending = match ssh_shell.pending_interactive.lock() {
                Ok(guard) => guard,
                Err(_) => {
                    mark_session_corrupted(ssh_shell, "interactive lock poisoned in Execute");
                    return Err("Interactive state lock poisoned");
                }
            };
            if pending.is_some() {
                shlog_trace!("New command received while interactive command was pending, sending Ctrl+C");
                write_to_channel(ssh_shell, &[3])?;
                std::thread::sleep(Duration::from_millis(300));
                *pending = None;
            }
        }

        // Clear shared buffer and reset monotonic counter before sending command
        {
            let mut shared_buffer = match ssh_shell.output_buffer.lock() {
                Ok(guard) => guard,
                Err(_) => {
                    mark_session_corrupted(ssh_shell, "buffer lock poisoned before command");
                    return Err("Buffer lock poisoned");
                }
            };
            shared_buffer.clear();
            ssh_shell.total_bytes_written.store(0, Ordering::Release);
            ssh_shell.read_position.store(0, Ordering::Release);
            shlog_trace!("Cleared shared buffer and reset counters before command");
        }

        // Send command
        let cmd_with_newline = format!("{}\n", cmd);
        write_to_channel(ssh_shell, cmd_with_newline.as_bytes())?;

        // Read output using monotonic byte counter
        let mut output_buffer = Vec::new();
        let mut last_total_bytes = 0usize;
        let mut no_data_count = 0;
        let mut prompt_detected = false;
        let mut was_truncated = false;
        let mut seen_command_echo = false;

        // Scale silence threshold with timeout: at least 1/3 of max_iterations,
        // but no less than the default 3 seconds. This prevents false
        // requires_interaction on slow commands (network I/O, compilation, etc.)
        let silence_threshold = std::cmp::max(
            INTERACTIVE_DETECTION_ITERATIONS,
            max_iterations / 3,
        );

        shlog_trace!("Starting to read command output from shared buffer (silence_threshold={})", silence_threshold);
        std::thread::sleep(Duration::from_millis(COMMAND_OUTPUT_WAIT_MS));

        for iteration in 0..max_iterations {
            let current_total = ssh_shell.total_bytes_written.load(Ordering::Acquire);
            let has_new_data = current_total > last_total_bytes;

            if has_new_data {
                let snapshot = {
                    let shared_buffer = ssh_shell.output_buffer.lock()
                        .map_err(|_| "Buffer lock poisoned")?;
                    shared_buffer.clone()
                };

                output_buffer = snapshot;
                last_total_bytes = current_total;

                if truncate_to_tail(&mut output_buffer, max_output_bytes) {
                    was_truncated = true;
                    shlog_trace!("Output buffer truncated to tail ({} bytes)", max_output_bytes);
                }

                let output_str = String::from_utf8_lossy(&output_buffer);

                // Track whether the command echo-back has been received.
                // The shell echoes the command text followed by a newline before
                // producing actual output. Don't start counting silence until
                // we've seen this echo, to avoid false requires_interaction on
                // commands that are slow to produce their first real output.
                if !seen_command_echo && output_str.contains('\n') {
                    seen_command_echo = true;
                    shlog_trace!("Command echo-back detected");
                }

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
                    shlog_trace!("Interactive prompt pattern detected early: {:?}", output_str.lines().last());
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

                // Fallback interactive detection after silence threshold expires.
                // Only trigger if we've seen the command echo-back — before that,
                // the command hasn't had a chance to produce output yet.
                if no_data_count >= silence_threshold && !output_buffer.is_empty() && seen_command_echo {
                    let output_str = String::from_utf8_lossy(&output_buffer);

                    if is_prompt_or_marker(&output_str, marker) {
                        prompt_detected = true;
                        shlog_trace!("Prompt detected on final check");
                        break;
                    }

                    if is_interactive_prompt(&output_str) {
                        shlog_trace!("Interactive prompt pattern detected: {:?}", output_str.lines().last());
                        break;
                    }

                    shlog_trace!("Command appears to be interactive (no new data for {}ms, no prompt detected)",
                        silence_threshold as u64 * ITERATION_SLEEP_MS);
                    break;
                }
            }
            std::thread::sleep(Duration::from_millis(ITERATION_SLEEP_MS));
        }

        // Build result table
        let mut result_table = AutoTableVar::new();
        let output_str = String::from_utf8_lossy(&output_buffer).to_string();

        if prompt_detected {
            let exit_code = capture_exit_code(ssh_shell, marker);

            let final_output = if should_clean {
                clean_output_with_marker(&output_str, marker)
            } else {
                output_str
            };

            result_table.0.insert_fast_static("status", &Var::ephemeral_string("completed"));
            result_table.0.insert_fast_static("output", &Var::ephemeral_string(&final_output));
            if let Some(code) = exit_code {
                result_table.0.insert_fast_static("exit_code", &Var::from(code));
            }
            if was_truncated {
                result_table.0.insert_fast_static("truncated", &true.into());
            }
        } else {
            {
                let mut pending = ssh_shell.pending_interactive.lock()
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

            result_table.0.insert_fast_static("status", &Var::ephemeral_string("requires_interaction"));
            result_table.0.insert_fast_static("output", &Var::ephemeral_string(&partial_output));
            result_table.0.insert_fast_static(
                "message",
                &Var::ephemeral_string("Command is waiting for input. Use SSH.SendInput to interact.")
            );
            if was_truncated {
                result_table.0.insert_fast_static("truncated", &true.into());
            }
        }

        // Sync read position
        let final_total = ssh_shell.total_bytes_written.load(Ordering::Acquire);
        ssh_shell.read_position.store(final_total, Ordering::Release);

        self.output = result_table.to_cloned();
        Ok(self.output.0)
    }
}

// ============================================================================
// SSH.SendInput Shard
// ============================================================================

#[derive(shards::shard)]
#[shard_info(
    "SSH.SendInput",
    "Send input to an interactive SSH command"
)]
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
        &EXECUTE_OUTPUT_TYPES
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
        let max_output_bytes = if max_output_bytes <= 0 { usize::MAX } else { max_output_bytes as usize };

        let timeout_secs: i64 = self.timeout_secs.get().as_ref().try_into()?;
        let timeout_secs = if timeout_secs <= 0 { 10 } else { timeout_secs };
        let max_iterations = (timeout_secs as usize) * 10;

        let ssh_shell = get_session(&session_var)?;
        check_session_alive(ssh_shell)?;

        let marker = ssh_shell.prompt_marker.as_deref();

        // Check if there's a pending interactive command
        {
            let pending = match ssh_shell.pending_interactive.lock() {
                Ok(guard) => guard,
                Err(_) => {
                    mark_session_corrupted(ssh_shell, "interactive lock poisoned in SendInput");
                    return Err("Interactive state lock poisoned");
                }
            };
            if pending.is_none() {
                return Err("No interactive command is pending");
            }
        }

        // Record monotonic position before sending input
        let start_total_bytes = ssh_shell.total_bytes_written.load(Ordering::Acquire);

        // Send input (if not empty)
        if !input_str.is_empty() {
            let input_with_newline = format!("{}\n", input_str);
            shlog_trace!("SendInput: Writing {} bytes: {:?}", input_with_newline.len(), input_with_newline);
            write_to_channel(ssh_shell, input_with_newline.as_bytes())?;
            shlog_trace!("SendInput: Write succeeded");
        }

        // Wait for output
        std::thread::sleep(Duration::from_millis(SENDINPUT_INITIAL_WAIT_MS));
        shlog_trace!("SendInput: Starting to read output after input (start_total_bytes={})", start_total_bytes);

        let mut output_buffer = Vec::new();
        let mut last_total_bytes = start_total_bytes;
        let mut was_truncated = false;
        let mut no_data_count = 0;
        let mut prompt_detected = false;

        for iteration in 0..max_iterations {
            let current_total = ssh_shell.total_bytes_written.load(Ordering::Acquire);
            let has_new_data = current_total > last_total_bytes;

            if has_new_data {
                let snapshot = {
                    let shared_buffer = ssh_shell.output_buffer.lock()
                        .map_err(|_| "Buffer lock poisoned")?;
                    shared_buffer.clone()
                };

                output_buffer = snapshot;
                last_total_bytes = current_total;

                if truncate_to_tail(&mut output_buffer, max_output_bytes) {
                    was_truncated = true;
                }

                let output_str = String::from_utf8_lossy(&output_buffer);
                shlog_trace!(
                    "SendInput: Read data (iteration {}), total_bytes: {}, buffer size: {}",
                    iteration, current_total, output_buffer.len()
                );

                no_data_count = 0;

                if is_prompt_or_marker(&output_str, marker) {
                    shlog_trace!("SendInput: Prompt detected in new output, exiting early");
                    prompt_detected = true;
                    break;
                }
            } else {
                no_data_count += 1;
                shlog_trace!("SendInput: No new data (iteration {}), no_data_count: {}",
                    iteration, no_data_count);

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

        // Final prompt check
        if !prompt_detected {
            let output_str = String::from_utf8_lossy(&output_buffer);
            prompt_detected = is_prompt_or_marker(&output_str, marker);
        }

        // Clean output
        let output_str = String::from_utf8_lossy(&output_buffer).to_string();
        let stripped_bytes = strip_ansi_escapes::strip(&output_str);
        let cleaned = String::from_utf8_lossy(&stripped_bytes);
        let mut lines: Vec<&str> = cleaned.lines().collect();
        if prompt_detected && !lines.is_empty() {
            lines.pop();
        }
        let final_output = lines.join("\n");

        let mut result_table = AutoTableVar::new();

        if prompt_detected {
            {
                let mut pending = ssh_shell.pending_interactive.lock()
                    .map_err(|_| "Interactive state lock poisoned")?;
                *pending = None;
            }
            shlog_trace!("Interactive session completed");
            result_table.0.insert_fast_static("status", &Var::ephemeral_string("completed"));
            result_table.0.insert_fast_static("output", &Var::ephemeral_string(&final_output));
            if was_truncated {
                result_table.0.insert_fast_static("truncated", &true.into());
            }
        } else {
            result_table.0.insert_fast_static("status", &Var::ephemeral_string("pending_output"));
            result_table.0.insert_fast_static("output", &Var::ephemeral_string(&final_output));
            result_table.0.insert_fast_static(
                "message",
                &Var::ephemeral_string("Command still running. Use SSH.SendInput to continue.")
            );
            if was_truncated {
                result_table.0.insert_fast_static("truncated", &true.into());
            }
        }

        // Sync read position
        let final_total = ssh_shell.total_bytes_written.load(Ordering::Acquire);
        ssh_shell.read_position.store(final_total, Ordering::Release);

        self.output = result_table.to_cloned();
        Ok(self.output.0)
    }
}

// ============================================================================
// SSH.IsConnected Shard
// ============================================================================

#[derive(shards::shard)]
#[shard_info(
    "SSH.IsConnected",
    "Check if SSH session is still connected"
)]
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
        let ssh_shell = get_session(input)?;
        let is_connected = ssh_shell.is_connected.load(Ordering::Acquire);
        self.output = is_connected.into();
        Ok(Some(self.output.0))
    }
}

// ============================================================================
// SSH.Read Shard — raw non-blocking read
// ============================================================================

#[derive(shards::shard)]
#[shard_info(
    "SSH.Read",
    "Read raw output from SSH session (non-blocking)"
)]
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
        let max_bytes = if max_bytes <= 0 { 65536usize } else { max_bytes as usize };
        let strip: bool = self.strip_ansi.get().as_ref().try_into()?;
        let timeout_ms: i64 = self.timeout_ms.get().as_ref().try_into()?;
        let timeout_ms = if timeout_ms < 0 { 0u64 } else { timeout_ms as u64 };

        let ssh_shell = get_session(&session_var)?;
        check_session_alive(ssh_shell)?;

        let read_pos = &ssh_shell.read_position;

        let deadline = if timeout_ms > 0 {
            Some(std::time::Instant::now() + Duration::from_millis(timeout_ms))
        } else {
            None
        };

        let mut result_data = Vec::new();

        // Settle time scales with timeout
        let settle_ms = if timeout_ms > 0 {
            (timeout_ms / 5).max(100).min(2000)
        } else {
            100
        };
        let mut last_data_time: Option<std::time::Instant> = None;
        let mut prev_total = read_pos.load(Ordering::Acquire);

        loop {
            let current_total = ssh_shell.total_bytes_written.load(Ordering::Acquire);

            if current_total > prev_total {
                last_data_time = Some(std::time::Instant::now());
                prev_total = current_total;
            }

            if let Some(dl) = deadline {
                if std::time::Instant::now() >= dl {
                    break;
                }
                if let Some(ldt) = last_data_time {
                    if std::time::Instant::now().duration_since(ldt) >= Duration::from_millis(settle_ms) {
                        break;
                    }
                }
                shards::core::suspend(context, 0.01);
            } else {
                break;
            }
        }

        // Final snapshot of accumulated data
        let final_total = ssh_shell.total_bytes_written.load(Ordering::Acquire);
        let final_consumed = read_pos.load(Ordering::Acquire);
        if final_total > final_consumed {
            let snapshot = {
                let buf = ssh_shell.output_buffer.lock()
                    .map_err(|_| "Buffer lock poisoned")?;
                buf.clone()
            };

            let new_byte_count = final_total - final_consumed;
            let available = snapshot.len().min(new_byte_count).min(max_bytes);
            result_data = snapshot[snapshot.len().saturating_sub(available)..].to_vec();
            read_pos.store(final_total, Ordering::Release);
        }

        let output_str = if strip {
            let term_rows = ssh_shell.term_rows.load(Ordering::Acquire);
            let term_cols = ssh_shell.term_cols.load(Ordering::Acquire);
            render_through_virtual_terminal(&result_data, term_rows, term_cols)
        } else {
            String::from_utf8_lossy(&result_data).to_string()
        };

        self.output = Var::ephemeral_string(&output_str).into();
        Ok(Some(self.output.0))
    }
}

// ============================================================================
// SSH.Write Shard — raw byte write
// ============================================================================

#[derive(shards::shard)]
#[shard_info(
    "SSH.Write",
    "Write raw bytes to SSH session"
)]
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

        let ssh_shell = get_session(&session_var)?;
        check_session_alive(ssh_shell)?;

        let bytes = interpret_escape_sequences(input_str);
        write_to_channel(ssh_shell, &bytes)?;
        if append_nl {
            write_to_channel(ssh_shell, b"\r")?;
        }

        // Passthrough input
        self.output = input.into();
        Ok(Some(self.output.0))
    }
}

// ============================================================================
// SSH.Resize Shard — PTY resize
// ============================================================================

#[derive(shards::shard)]
#[shard_info(
    "SSH.Resize",
    "Resize the PTY terminal of an SSH session"
)]
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

        let rows = rows_val.max(1).min(u16::MAX as i64) as u16;
        let cols = cols_val.max(1).min(u16::MAX as i64) as u16;

        let ssh_shell = get_session(&session_var)?;
        check_session_alive(ssh_shell)?;

        // Set blocking for the resize request
        if let Ok(sess) = ssh_shell.session.lock() {
            sess.set_blocking(true);
        }

        {
            let mut channel = ssh_shell.channel.lock()
                .map_err(|_| "Channel lock poisoned")?;
            channel.request_pty_size(cols as u32, rows as u32, None, None)
                .map_err(|_| "Failed to resize PTY")?;
        }

        // Restore non-blocking
        if let Ok(sess) = ssh_shell.session.lock() {
            sess.set_blocking(false);
        }

        ssh_shell.term_rows.store(rows, Ordering::Release);
        ssh_shell.term_cols.store(cols, Ordering::Release);
        shlog_trace!("SSH PTY resized to {}x{}", cols, rows);

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
