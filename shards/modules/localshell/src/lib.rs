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

use portable_pty::{CommandBuilder, PtySize};
use std::io::{Read, Write};
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

// LocalShell session object wrapper
mod local_shell {
    use super::*;

    #[derive(Debug, Clone, PartialEq)]
    pub enum SessionState {
        Running,
        ProcessDied,
        Corrupted(String),
    }

    pub struct LocalShellSession {
        pub pair: Arc<Mutex<Option<portable_pty::PtyPair>>>,
        #[allow(dead_code)] // Kept alive for Drop ordering; reader arc is moved into thread
        pub reader: Arc<Mutex<Box<dyn Read + Send>>>,
        pub writer: Arc<Mutex<Box<dyn Write + Send>>>,
        pub pending_interactive: Arc<Mutex<Option<InteractiveState>>>,
        pub is_alive: Arc<AtomicBool>,
        pub state: Arc<Mutex<SessionState>>,
        pub output_buffer: Arc<Mutex<Vec<u8>>>,
        pub total_bytes_written: Arc<AtomicUsize>,
        /// Read cursor for LocalShell.Read — tracks how many bytes have been consumed
        /// by raw reads. Lives on the session so multiple Read shard instances share state.
        pub read_position: Arc<AtomicUsize>,
        pub reader_thread: Arc<Mutex<Option<std::thread::JoinHandle<()>>>>,
        pub child: Arc<Mutex<Option<Box<dyn portable_pty::Child + Send + Sync>>>>,
        pub prompt_marker: Option<String>,
        pub term_rows: Arc<AtomicU16>,
        pub term_cols: Arc<AtomicU16>,
    }

    #[derive(Clone)]
    pub struct InteractiveState {
        #[allow(dead_code)] // Stored for debugging/future use
        pub original_cmd: String,
    }

    impl Drop for LocalShellSession {
        fn drop(&mut self) {
            shlog_trace!("Dropping LocalShellSession, cleaning up");

            // Step 1: Mark as not alive (prevents new operations)
            self.is_alive.store(false, Ordering::Release);

            // Step 2: Kill the shell child process
            if let Ok(mut child_opt) = self.child.lock() {
                if let Some(mut child) = child_opt.take() {
                    shlog_trace!("Killing shell child process");
                    let _ = child.kill();
                    let _ = child.wait();
                }
            }

            // Step 3: CRITICAL - Drop the PTY pair to close file descriptors
            // This causes the blocking read() in the reader thread to return with EOF
            if let Ok(mut pair_opt) = self.pair.lock() {
                if let Some(pair) = pair_opt.take() {
                    shlog_trace!("Dropping PTY pair to close file descriptors");
                    drop(pair);
                    shlog_trace!("PTY pair dropped");
                }
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

            shlog_trace!("LocalShellSession cleanup complete");
        }
    }

    ref_counted_object_type_impl!(LocalShellSession);
}

use local_shell::*;

lazy_static! {
    static ref LOCAL_SHELL_TYPE: Type = Type::object(FRAG_CC, fourCharacterCode(*b"lshl"));
    static ref LOCAL_SHELL_TYPE_VEC: Vec<Type> = vec![*LOCAL_SHELL_TYPE];
    static ref LOCAL_SHELL_VAR_TYPE: Type = Type::context_variable(&LOCAL_SHELL_TYPE_VEC);
    static ref EXECUTE_OUTPUT_TYPES: Vec<Type> = vec![common_type::string_table];
}

// Helper: detect shell prompts
fn is_prompt(text: &str) -> bool {
    is_prompt_or_marker(text, None)
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
fn mark_session_corrupted(local_shell: &LocalShellSession, error_context: &str) {
    shlog_error!("Critical error in LocalShell ({}), marking session as corrupted", error_context);
    local_shell.is_alive.store(false, Ordering::Release);
    if let Ok(mut state) = local_shell.state.lock() {
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

// Helper: check if session is alive, return descriptive error if not
fn check_session_alive(local_shell: &LocalShellSession) -> Result<(), &'static str> {
    if !local_shell.is_alive.load(Ordering::Acquire) {
        if let Ok(state) = local_shell.state.lock() {
            return Err(match *state {
                SessionState::ProcessDied => "Local shell process has exited",
                SessionState::Corrupted(ref reason) => {
                    shlog_error!("Session corrupted: {}", reason);
                    "Local shell session corrupted due to internal error"
                },
                SessionState::Running => "Local shell is not alive (unexpected state)",
            });
        } else {
            return Err("Local shell session corrupted (state lock poisoned)");
        }
    }
    Ok(())
}

// Helper: extract LocalShellSession from Var
fn get_session(session_var: &Var) -> Result<&LocalShellSession, &'static str> {
    let local_shell = unsafe {
        Var::from_ref_counted_object::<LocalShellSession>(session_var, &*LOCAL_SHELL_TYPE)?
    };
    Ok(unsafe { &*(local_shell as *const LocalShellSession) })
}

// Start the reader thread for a session's shared buffer
fn start_reader_thread(
    reader_arc: Arc<Mutex<Box<dyn Read + Send>>>,
    buffer: Arc<Mutex<Vec<u8>>>,
    total_bytes: Arc<AtomicUsize>,
    alive: Arc<AtomicBool>,
    state: Arc<Mutex<SessionState>>,
) -> std::thread::JoinHandle<()> {
    std::thread::spawn(move || {
        shlog_trace!("Reader thread started");
        let mut buf = [0u8; 4096];

        loop {
            if !alive.load(Ordering::Acquire) {
                shlog_trace!("Reader thread exiting");
                break;
            }

            match reader_arc.lock() {
                Ok(mut reader) => {
                    match reader.read(&mut buf) {
                        Ok(n) if n > 0 => {
                            match buffer.lock() {
                                Ok(mut b) => {
                                    b.extend_from_slice(&buf[..n]);

                                    if b.len() > MAX_BUFFER_BYTES {
                                        if truncate_to_tail(&mut b, MAX_BUFFER_BYTES) {
                                            shlog_trace!("Reader thread: buffer truncated to {} bytes", b.len());
                                        }
                                    }

                                    // Increment monotonic counter AFTER write
                                    total_bytes.fetch_add(n, Ordering::Release);

                                    shlog_trace!("Reader thread: read {} bytes, buffer now {} bytes", n, b.len());
                                }
                                Err(e) => {
                                    shlog_trace!("Reader thread: buffer lock poisoned: {}", e);
                                    break;
                                }
                            }
                        }
                        Ok(_) => {
                            alive.store(false, Ordering::Release);
                            if let Ok(mut s) = state.lock() {
                                *s = SessionState::ProcessDied;
                            }
                            shlog_trace!("Reader thread: EOF detected, process died");
                            break;
                        }
                        Err(e) => {
                            alive.store(false, Ordering::Release);
                            if let Ok(mut s) = state.lock() {
                                *s = SessionState::ProcessDied;
                            }
                            shlog_trace!("Reader thread: read error: {}", e);
                            break;
                        }
                    }
                }
                Err(e) => {
                    shlog_trace!("Reader thread: reader lock poisoned: {}", e);
                    break;
                }
            }
        }
        shlog_trace!("Reader thread finished");
    })
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
        let rows: u16 = if rows_val.is_none() { 24 } else {
            let v: i64 = rows_val.as_ref().try_into().unwrap_or(24);
            v.max(1).min(u16::MAX as i64) as u16
        };
        let cols: u16 = if cols_val.is_none() { 80 } else {
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

        shlog_trace!("Creating local shell with: {} ({}x{})", shell_cmd, cols, rows);

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
            let args_seq: shards::types::SeqVar = shell_args_var.as_ref().try_into()
                .map_err(|_| "ShellArgs must be a sequence of strings")?;
            for arg_var in args_seq.iter() {
                let arg_str = match arg_var.as_ref() {
                    Var { valueType: shards::shardsc::SHType_String, .. } => {
                        let s: &str = arg_var.as_ref().try_into()
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

        let reader = pair.master.try_clone_reader()
            .map_err(|_| "Failed to clone reader")?;
        let writer = pair.master.take_writer()
            .map_err(|_| "Failed to take writer")?;

        let reader_arc = Arc::new(Mutex::new(reader));
        let writer_arc = Arc::new(Mutex::new(writer));

        // Create shared state FIRST
        let output_buffer = Arc::new(Mutex::new(Vec::new()));
        let total_bytes_written = Arc::new(AtomicUsize::new(0));
        let is_alive = Arc::new(AtomicBool::new(true));
        let state = Arc::new(Mutex::new(SessionState::Running));

        // Start reader thread BEFORE waiting for initial prompt (Bug 1 fix)
        let reader_thread = start_reader_thread(
            Arc::clone(&reader_arc),
            Arc::clone(&output_buffer),
            Arc::clone(&total_bytes_written),
            Arc::clone(&is_alive),
            Arc::clone(&state),
        );

        // Wait for initial prompt by polling the shared buffer (no throwaway threads)
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

        shlog_trace!("Shell ready");

        // Try to set PS1 sentinel marker for reliable prompt detection
        let marker = format!("__SHARDS_PROMPT_{:x}__", std::time::SystemTime::now()
            .duration_since(std::time::UNIX_EPOCH)
            .unwrap_or_default()
            .as_nanos() & 0xFFFFFFFF);

        // Send PS1 setup command
        let prompt_marker = {
            let ps1_cmd = format!("export PS1='{}'\n", marker);
            let mut set_marker = false;
            if let Ok(mut w) = writer_arc.lock() {
                if w.write_all(ps1_cmd.as_bytes()).is_ok() && w.flush().is_ok() {
                    set_marker = true;
                }
            }

            if set_marker {
                // Wait for the marker to appear
                std::thread::sleep(Duration::from_millis(300));

                // Check if marker appeared in buffer
                let marker_found = if let Ok(buf) = output_buffer.lock() {
                    let s = String::from_utf8_lossy(&buf);
                    s.contains(&marker)
                } else {
                    false
                };

                if marker_found {
                    shlog_trace!("Sentinel prompt marker set: {}", marker);
                    // Clear buffer after marker setup - we don't want setup output in command results
                    if let Ok(mut buf) = output_buffer.lock() {
                        buf.clear();
                    }
                    total_bytes_written.store(0, Ordering::Release);
                    Some(marker)
                } else {
                    shlog_trace!("Sentinel prompt marker not detected, falling back to pattern matching");
                    // Clear buffer anyway
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

        let local_shell = LocalShellSession {
            pair: Arc::new(Mutex::new(Some(pair))),
            reader: reader_arc,
            writer: writer_arc,
            pending_interactive: Arc::new(Mutex::new(None)),
            is_alive,
            state,
            output_buffer,
            total_bytes_written,
            read_position: Arc::new(AtomicUsize::new(0)),
            reader_thread: Arc::new(Mutex::new(Some(reader_thread))),
            child: Arc::new(Mutex::new(Some(child))),
            prompt_marker,
            term_rows: Arc::new(AtomicU16::new(rows)),
            term_cols: Arc::new(AtomicU16::new(cols)),
        };

        let shell_var = Var::new_ref_counted(local_shell, &*LOCAL_SHELL_TYPE);
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

        let local_shell = get_session(&session_var)?;
        check_session_alive(local_shell)?;

        let marker = local_shell.prompt_marker.as_deref();

        // Check if there's a pending interactive command
        {
            let mut pending = match local_shell.pending_interactive.lock() {
                Ok(guard) => guard,
                Err(_) => {
                    mark_session_corrupted(local_shell, "interactive lock poisoned in Execute");
                    return Err("Interactive state lock poisoned");
                }
            };
            if pending.is_some() {
                shlog_trace!("New command received while interactive command was pending, sending Ctrl+C");
                {
                    let mut writer = match local_shell.writer.lock() {
                        Ok(guard) => guard,
                        Err(_) => {
                            mark_session_corrupted(local_shell, "writer lock poisoned in Execute");
                            return Err("Writer lock poisoned");
                        }
                    };
                    let _ = writer.write_all(&[3]);
                    let _ = writer.flush();
                }
                std::thread::sleep(Duration::from_millis(300));
                *pending = None;
            }
        }

        // Clear shared buffer and reset monotonic counter before sending command
        {
            let mut shared_buffer = match local_shell.output_buffer.lock() {
                Ok(guard) => guard,
                Err(_) => {
                    mark_session_corrupted(local_shell, "buffer lock poisoned before command");
                    return Err("Buffer lock poisoned");
                }
            };
            shared_buffer.clear();
            local_shell.total_bytes_written.store(0, Ordering::Release);
            local_shell.read_position.store(0, Ordering::Release);
            shlog_trace!("Cleared shared buffer and reset counters before command");
        }

        // Send command
        let cmd_with_newline = format!("{}\n", cmd);
        {
            let mut writer = match local_shell.writer.lock() {
                Ok(guard) => guard,
                Err(_) => {
                    mark_session_corrupted(local_shell, "writer lock poisoned sending command");
                    return Err("Writer lock poisoned");
                }
            };
            writer.write_all(cmd_with_newline.as_bytes())
                .map_err(|_| "Failed to write command to shell (IO error)")?;
            writer.flush()
                .map_err(|_| "Failed to flush command writer (IO error)")?;
        }

        // Read output using monotonic byte counter (Bug 2 fix)
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
            // Use monotonic counter to detect new data (immune to truncation)
            let current_total = local_shell.total_bytes_written.load(Ordering::Acquire);
            let has_new_data = current_total > last_total_bytes;

            if has_new_data {
                // Mark that the command has started producing output.
                // Done before truncation so that small MaxOutputBytes or
                // fast/large output can't cause us to miss the signal.
                if !seen_command_echo {
                    seen_command_echo = true;
                    shlog_trace!("Command has started producing output");
                }

                // New data arrived — snapshot the buffer contents
                let snapshot = {
                    let shared_buffer = local_shell.output_buffer.lock()
                        .map_err(|_| "Buffer lock poisoned")?;
                    shared_buffer.clone()
                };

                output_buffer = snapshot;
                last_total_bytes = current_total;

                // Truncate local copy if needed
                if truncate_to_tail(&mut output_buffer, max_output_bytes) {
                    was_truncated = true;
                    shlog_trace!("Output buffer truncated to tail ({} bytes)", max_output_bytes);
                }

                let output_str = String::from_utf8_lossy(&output_buffer);

                shlog_trace!(
                    "Read data (iteration {}), total_bytes: {}, buffer size: {}, last line: {:?}",
                    iteration,
                    current_total,
                    output_buffer.len(),
                    output_str.lines().last()
                );

                // Check for prompt
                if is_prompt_or_marker(&output_str, marker) {
                    prompt_detected = true;
                    shlog_trace!("Prompt detected in output, command completed");
                    break;
                }

                // Check for interactive prompt IMMEDIATELY when we have data
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
                // Only trigger if the command has started producing output —
                // before that, the command hasn't had a chance to run yet.
                if no_data_count >= silence_threshold && seen_command_echo {
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
            // Try to capture exit code
            let exit_code = capture_exit_code(local_shell, marker);

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
            // Command is interactive (waiting for input)
            {
                let mut pending = local_shell.pending_interactive.lock()
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
                &Var::ephemeral_string("Command is waiting for input. Use LocalShell.SendInput to interact.")
            );
            if was_truncated {
                result_table.0.insert_fast_static("truncated", &true.into());
            }
        }

        // Sync read position so subsequent LocalShell.Read doesn't pick up stale data
        // from command execution or exit code capture
        let final_total = local_shell.total_bytes_written.load(Ordering::Acquire);
        local_shell.read_position.store(final_total, Ordering::Release);

        self.output = result_table.to_cloned();
        Ok(self.output.0)
    }
}

// Helper: capture exit code by sending `echo $?` after command completes
fn capture_exit_code(local_shell: &LocalShellSession, marker: Option<&str>) -> Option<i64> {
    // Clear buffer, send echo $?, wait for prompt, parse result
    {
        let mut buf = match local_shell.output_buffer.lock() {
            Ok(b) => b,
            Err(_) => return None,
        };
        buf.clear();
        local_shell.total_bytes_written.store(0, Ordering::Release);
        local_shell.read_position.store(0, Ordering::Release);
    }

    {
        let mut writer = match local_shell.writer.lock() {
            Ok(w) => w,
            Err(_) => return None,
        };
        if writer.write_all(b"echo $?\n").is_err() || writer.flush().is_err() {
            return None;
        }
    }

    // Wait for response
    std::thread::sleep(Duration::from_millis(200));

    for _ in 0..10 {
        let snapshot = {
            let buf = local_shell.output_buffer.lock().ok()?;
            buf.clone()
        };

        let output = String::from_utf8_lossy(&snapshot);
        if is_prompt_or_marker(&output, marker) {
            // Parse exit code from output
            let stripped = strip_ansi_escapes::strip(output.as_bytes());
            let cleaned = String::from_utf8_lossy(&stripped);
            for line in cleaned.lines() {
                let trimmed = line.trim();
                // Skip the echo command itself and the prompt
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

        let local_shell = get_session(&session_var)?;
        check_session_alive(local_shell)?;

        let marker = local_shell.prompt_marker.as_deref();

        // Check if there's a pending interactive command
        {
            let pending = match local_shell.pending_interactive.lock() {
                Ok(guard) => guard,
                Err(_) => {
                    mark_session_corrupted(local_shell, "interactive lock poisoned in SendInput");
                    return Err("Interactive state lock poisoned");
                }
            };
            if pending.is_none() {
                return Err("No interactive command is pending");
            }
        }

        // Record monotonic position before sending input
        let start_total_bytes = local_shell.total_bytes_written.load(Ordering::Acquire);

        // Send input (if not empty)
        if !input_str.is_empty() {
            let input_with_newline = format!("{}\n", input_str);
            shlog_trace!("SendInput: Writing {} bytes: {:?}", input_with_newline.len(), input_with_newline);
            let mut writer = match local_shell.writer.lock() {
                Ok(guard) => guard,
                Err(_) => {
                    mark_session_corrupted(local_shell, "writer lock poisoned in SendInput");
                    return Err("Writer lock poisoned");
                }
            };
            writer.write_all(input_with_newline.as_bytes())
                .map_err(|_| "Failed to write input to interactive command (IO error)")?;
            writer.flush()
                .map_err(|_| "Failed to flush input writer (IO error)")?;
            shlog_trace!("SendInput: Write and flush succeeded");
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
            let current_total = local_shell.total_bytes_written.load(Ordering::Acquire);
            let has_new_data = current_total > last_total_bytes;

            if has_new_data {
                // Snapshot the entire buffer
                let snapshot = {
                    let shared_buffer = local_shell.output_buffer.lock()
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
                    // Re-check buffer for prompt
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
            lines.pop(); // Remove prompt line
        }
        let final_output = lines.join("\n");

        let mut result_table = AutoTableVar::new();

        if prompt_detected {
            {
                let mut pending = local_shell.pending_interactive.lock()
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
                &Var::ephemeral_string("Command still running. Use LocalShell.SendInput to continue.")
            );
            if was_truncated {
                result_table.0.insert_fast_static("truncated", &true.into());
            }
        }

        // Sync read position so subsequent LocalShell.Read doesn't pick up stale data
        let final_total = local_shell.total_bytes_written.load(Ordering::Acquire);
        local_shell.read_position.store(final_total, Ordering::Release);

        self.output = result_table.to_cloned();
        Ok(self.output.0)
    }
}

// ============================================================================
// LocalShell.IsAlive Shard
// ============================================================================

#[derive(shards::shard)]
#[shard_info(
    "LocalShell.IsAlive",
    "Check if local shell session is still alive"
)]
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
        let local_shell = get_session(input)?;
        let is_alive = local_shell.is_alive.load(Ordering::Acquire);
        self.output = is_alive.into();
        Ok(Some(self.output.0))
    }
}

/// Render raw PTY output through a virtual VT100 terminal to get properly-spaced text.
/// Ink/TUI frameworks use cursor positioning sequences (CSI H, CSI C, etc.) for layout
/// instead of literal spaces — simple ANSI stripping concatenates words without gaps.
/// This processes bytes through a virtual screen and extracts the resulting text grid.
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
        let max_bytes = if max_bytes <= 0 { 65536usize } else { max_bytes as usize };
        let strip: bool = self.strip_ansi.get().as_ref().try_into()?;
        let timeout_ms: i64 = self.timeout_ms.get().as_ref().try_into()?;
        let timeout_ms = if timeout_ms < 0 { 0u64 } else { timeout_ms as u64 };

        let local_shell = get_session(&session_var)?;
        check_session_alive(local_shell)?;

        // Read cursor lives on the session so multiple Read shard instances share it
        let read_pos = &local_shell.read_position;

        let deadline = if timeout_ms > 0 {
            Some(std::time::Instant::now() + Duration::from_millis(timeout_ms))
        } else {
            None
        };

        let mut result_data = Vec::new();

        // Accumulate data until timeout expires or data settles.
        // Settle time scales with timeout: 20% of requested timeout, clamped [100ms, 2000ms].
        // TUI apps (Ink/React) render in bursts with gaps — a longer settle avoids
        // returning between render bursts with only partial output.
        let settle_ms = if timeout_ms > 0 {
            (timeout_ms / 5).max(100).min(2000)
        } else {
            100
        };
        let mut last_data_time: Option<std::time::Instant> = None;
        let mut prev_total = read_pos.load(Ordering::Acquire);

        loop {
            let current_total = local_shell.total_bytes_written.load(Ordering::Acquire);

            if current_total > prev_total {
                // New data arrived since last poll — reset settle timer
                last_data_time = Some(std::time::Instant::now());
                prev_total = current_total;
            }

            if let Some(dl) = deadline {
                // Hard timeout
                if std::time::Instant::now() >= dl {
                    break;
                }
                // Early exit: data arrived then went quiet for SETTLE_MS
                if let Some(ldt) = last_data_time {
                    if std::time::Instant::now().duration_since(ldt) >= Duration::from_millis(settle_ms) {
                        break;
                    }
                }
                // Yield to scheduler instead of blocking the thread
                shards::core::suspend(context, 0.01);
            } else {
                // No timeout = immediate mode, return whatever is available now
                break;
            }
        }

        // Final snapshot of accumulated data
        let final_total = local_shell.total_bytes_written.load(Ordering::Acquire);
        let final_consumed = read_pos.load(Ordering::Acquire);
        if final_total > final_consumed {
            let snapshot = {
                let buf = local_shell.output_buffer.lock()
                    .map_err(|_| "Buffer lock poisoned")?;
                buf.clone()
            };

            let new_byte_count = final_total - final_consumed;
            let available = snapshot.len().min(new_byte_count).min(max_bytes);
            result_data = snapshot[snapshot.len().saturating_sub(available)..].to_vec();
            read_pos.store(final_total, Ordering::Release);
        }

        let output_str = if strip {
            let term_rows = local_shell.term_rows.load(Ordering::Acquire);
            let term_cols = local_shell.term_cols.load(Ordering::Acquire);
            render_through_virtual_terminal(&result_data, term_rows, term_cols)
        } else {
            String::from_utf8_lossy(&result_data).to_string()
        };

        self.output = Var::ephemeral_string(&output_str).into();
        Ok(Some(self.output.0))
    }
}

// Interpret escape sequences in input strings for PTY writing.
// Handles \r, \n, \t, \\, \xNN. Also converts bare LF (0x0A) to CR (0x0D)
// because PTY Enter = CR, and TUI apps in raw mode expect CR not LF.
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
// LocalShell.Write Shard — raw byte write
// ============================================================================

#[derive(shards::shard)]
#[shard_info(
    "LocalShell.Write",
    "Write raw bytes to local shell session"
)]
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

        let local_shell = get_session(&session_var)?;
        check_session_alive(local_shell)?;

        {
            let bytes = interpret_escape_sequences(input_str);
            let mut writer = local_shell.writer.lock()
                .map_err(|_| "Writer lock poisoned")?;
            writer.write_all(&bytes)
                .map_err(|_| "Failed to write to shell (IO error)")?;
            if append_nl {
                writer.write_all(b"\r")
                    .map_err(|_| "Failed to write newline to shell (IO error)")?;
            }
            writer.flush()
                .map_err(|_| "Failed to flush writer (IO error)")?;
        }

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

        let rows = rows_val.max(1).min(u16::MAX as i64) as u16;
        let cols = cols_val.max(1).min(u16::MAX as i64) as u16;

        let local_shell = get_session(&session_var)?;
        check_session_alive(local_shell)?;

        let pair_guard = local_shell.pair.lock()
            .map_err(|_| "PTY pair lock poisoned")?;

        if let Some(ref pair) = *pair_guard {
            pair.master.resize(PtySize {
                rows,
                cols,
                pixel_width: 0,
                pixel_height: 0,
            }).map_err(|_| "Failed to resize PTY")?;
            local_shell.term_rows.store(rows, Ordering::Release);
            local_shell.term_cols.store(cols, Ordering::Release);
            shlog_trace!("PTY resized to {}x{}", cols, rows);
        } else {
            return Err("PTY pair has been dropped");
        }

        Ok(None)
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

    shlog_trace!("LocalShell module registered");
}
