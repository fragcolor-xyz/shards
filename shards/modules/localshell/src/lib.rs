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
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::mpsc::channel;
use std::time::Duration;

// Configuration constants
// These match the SSH module's patterns and timeouts
const INITIAL_PROMPT_WAIT_MS: u64 = 500;
const INITIAL_PROMPT_MAX_RETRIES: usize = 10;
const READER_THREAD_TIMEOUT_MS: u64 = 50;
const COMMAND_OUTPUT_WAIT_MS: u64 = 200;
const COMMAND_MAX_ITERATIONS: usize = 50;
// Interactive detection timeout: 3 seconds is a balance between responsiveness and false positives.
// This may be insufficient for systems under heavy load, slow I/O, or commands with slow startup.
// Consider making this configurable via a parameter in future iterations.
const INTERACTIVE_DETECTION_ITERATIONS: usize = 30; // 30 * 100ms = 3 seconds
const ITERATION_SLEEP_MS: u64 = 100;
const SENDINPUT_INITIAL_WAIT_MS: u64 = 500;
const SENDINPUT_MAX_ITERATIONS: usize = 30;
const SENDINPUT_NO_DATA_THRESHOLD: usize = 20; // 20 iterations = 2 seconds of no data before checking for prompt
const MAX_BUFFER_BYTES: usize = 65536; // 64KB default, matches SSH module
const BUFFER_TRUNCATE_KEEP_RATIO: usize = 93; // Keep 93% on truncation to avoid repeated truncation

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
        pub reader: Arc<Mutex<Box<dyn Read + Send>>>,
        pub writer: Arc<Mutex<Box<dyn Write + Send>>>,
        pub pending_interactive: Arc<Mutex<Option<InteractiveState>>>,
        pub is_alive: Arc<AtomicBool>,
        pub state: Arc<Mutex<SessionState>>,
        pub output_buffer: Arc<Mutex<Vec<u8>>>,
        pub reader_thread: Arc<Mutex<Option<std::thread::JoinHandle<()>>>>,
        pub child: Arc<Mutex<Option<Box<dyn portable_pty::Child + Send + Sync>>>>,
    }

    #[derive(Clone)]
    pub struct InteractiveState {
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
            // This MUST happen before we try to lock the reader, because:
            // - The reader thread holds reader lock while blocked in read()
            // - We can't get the lock while the thread is blocking
            // - Dropping the pair closes the master PTY FD
            // - This causes the blocking read() to return with EOF
            // - Then the reader thread releases the lock and exits
            if let Ok(mut pair_opt) = self.pair.lock() {
                if let Some(pair) = pair_opt.take() {
                    shlog_trace!("Dropping PTY pair to close file descriptors");
                    drop(pair);
                    shlog_trace!("PTY pair dropped");
                }
            }

            // Step 4: Wait for reader thread with timeout
            // The thread should now exit quickly since the PTY FD is closed
            if let Ok(mut thread_opt) = self.reader_thread.lock() {
                if let Some(thread) = thread_opt.take() {
                    shlog_trace!("Waiting for reader thread to finish");

                    // Give it 2 seconds to exit gracefully
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
                        // Don't join - let it leak rather than hang forever
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

// Helper function to check shared buffer for prompt in a specific range
// This prevents race conditions where the reader thread adds more data after we stop reading,
// and ensures we only check NEW output received after SendInput was called (not old data)
//
// IMPORTANT: If buffer truncation occurs, positions become invalid. This function detects
// truncation and returns false to avoid checking invalid ranges.
fn check_buffer_for_prompt(
    local_shell: &LocalShellSession,
    from_position: usize,
    up_to_position: usize
) -> Result<bool, &'static str> {
    let shared_buffer = local_shell.output_buffer.lock()
        .map_err(|_| "Buffer lock poisoned")?;

    // Check if buffer was truncated by looking for truncation message at the start
    // If truncated, positions are invalid, so return false
    if shared_buffer.starts_with(b"[... output truncated ...]") {
        shlog_trace!("Buffer was truncated, positions invalid, skipping shared buffer check");
        return Ok(false);
    }

    // Clamp positions to buffer size
    let safe_from = from_position.min(shared_buffer.len());
    let safe_to = up_to_position.min(shared_buffer.len());

    // Only check the relevant range (data received since from_position)
    if safe_to <= safe_from {
        return Ok(false); // No new data to check
    }

    // Additional safety: if positions indicate a large range that exceeds MAX_BUFFER_BYTES,
    // truncation likely occurred, invalidating positions
    if up_to_position - from_position > MAX_BUFFER_BYTES {
        shlog_trace!("Position range too large ({}..{}), likely truncated, skipping check",
            from_position, up_to_position);
        return Ok(false);
    }

    let relevant_output = &shared_buffer[safe_from..safe_to];
    let output_str = String::from_utf8_lossy(relevant_output);
    Ok(is_prompt(&output_str))
}

// Helper function to detect shell prompts (NOT interactive program prompts)
// Reused from SSH module
fn is_prompt(text: &str) -> bool {
    // Strip ANSI codes first before checking for prompt
    let stripped_bytes = strip_ansi_escapes::strip(text.as_bytes());
    let cleaned = String::from_utf8_lossy(&stripped_bytes);

    let lines: Vec<&str> = cleaned.lines().collect();
    if let Some(last) = lines.last() {
        let trimmed = last.trim();

        // Exclude interactive program prompts like >>> (Python), >> (continuation)
        // These indicate we're IN a program, not at the shell
        if trimmed.ends_with(">>>") || trimmed.ends_with(">>") {
            return false;
        }

        // Check for common SHELL prompt patterns only
        trimmed.ends_with("$ ")
            || trimmed.ends_with("$")
            || trimmed.ends_with("# ")
            || trimmed.ends_with("#")
            || trimmed.ends_with("> ")  // Windows/PowerShell prompt
            || trimmed.ends_with(">")   // Single > is OK (but not >> or >>>)
            || trimmed.ends_with("% ")  // zsh prompt
            || trimmed.ends_with("%")   // zsh prompt
    } else {
        false
    }
}

// Helper function to detect common interactive prompts (password, confirmation, etc.)
// These patterns indicate a command is waiting for user input
fn is_interactive_prompt(text: &str) -> bool {
    // Strip ANSI codes first
    let stripped_bytes = strip_ansi_escapes::strip(text.as_bytes());
    let cleaned = String::from_utf8_lossy(&stripped_bytes);

    // Check last few lines for interactive patterns
    let lines: Vec<&str> = cleaned.lines().collect();
    if lines.is_empty() {
        return false;
    }

    // Check last 3 lines (some prompts span multiple lines)
    let check_lines = if lines.len() > 3 { &lines[lines.len()-3..] } else { &lines[..] };

    for line in check_lines {
        let lower = line.to_lowercase();

        // Common interactive patterns
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

// Helper function to mark session as corrupted after encountering a critical error (like lock poisoning)
fn mark_session_corrupted(local_shell: &LocalShellSession, error_context: &str) {
    shlog_error!("Critical error in LocalShell ({}), marking session as corrupted", error_context);
    local_shell.is_alive.store(false, Ordering::Release);
    // Try to set state, but if the lock is poisoned, is_alive=false is enough
    if let Ok(mut state) = local_shell.state.lock() {
        *state = SessionState::Corrupted(error_context.to_string());
    }
}

// Helper function to clean output (remove ANSI codes, control chars, and trailing prompts)
// Reused from SSH module
fn clean_output(output: &str) -> String {
    // Strip ANSI escape sequences
    let stripped_bytes = strip_ansi_escapes::strip(output);
    let text = String::from_utf8_lossy(&stripped_bytes);

    // Remove control characters (except newlines and tabs)
    let no_control: String = text.chars()
        .filter(|c| !c.is_control() || *c == '\n' || *c == '\t')
        .collect();

    let mut lines: Vec<&str> = no_control.lines().collect();

    // Remove trailing prompt if present
    if !lines.is_empty() {
        let last_line = lines.last().unwrap();
        if is_prompt(last_line) {
            lines.pop();
        }
    }

    // Join lines and trim
    let result = lines.join("\n");
    result.trim().to_string()
}

// Helper function to truncate output buffer to keep tail
// Reused from SSH module
fn truncate_to_tail(buffer: &mut Vec<u8>, max_bytes: usize) -> bool {
    if buffer.len() <= max_bytes {
        return false;
    }

    // Keep last ~93% of max_bytes to avoid repeated truncation on every read
    let keep_bytes = (max_bytes * 93) / 100;
    let truncate_msg = b"[... output truncated ...]\n";

    // Remove from the front, keep the tail
    let skip = buffer.len() - keep_bytes + truncate_msg.len();
    let tail: Vec<u8> = buffer.drain(skip..).collect();
    buffer.clear();
    buffer.extend_from_slice(truncate_msg);
    buffer.extend_from_slice(&tail);

    true
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

    output: ClonedVar,
}

impl Default for CreateShard {
    fn default() -> Self {
        Self {
            required: ExposedTypes::new(),
            shell: ParamVar::new(Var::default()),
            working_dir: ParamVar::new(Var::default()),
            shell_args: ParamVar::new(Var::default()),
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

        // Get PTY system
        let pty_system = portable_pty::native_pty_system();

        // Determine shell command
        let shell_cmd = if !shell_var.is_none() {
            let shell_path: &str = shell_var.as_ref().try_into()?;
            shell_path.to_string()
        } else {
            // Default shell based on platform
            #[cfg(unix)]
            let default_shell = "/bin/bash";
            #[cfg(windows)]
            let default_shell = "cmd.exe";

            default_shell.to_string()
        };

        shlog_trace!("Creating local shell with: {}", shell_cmd);

        // Create PTY pair
        let pair = pty_system
            .openpty(PtySize {
                rows: 24,
                cols: 80,
                pixel_width: 0,
                pixel_height: 0,
            })
            .map_err(|_| "Failed to open PTY")?;

        // Set up command
        let mut cmd = CommandBuilder::new(&shell_cmd);

        // Set working directory if specified
        if !working_dir_var.is_none() {
            let wd: &str = working_dir_var.as_ref().try_into()?;
            cmd.cwd(wd);
        }

        // Set shell arguments
        let shell_args_var = self.shell_args.get();
        if !shell_args_var.is_none() {
            // User provided custom arguments
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
            // Default arguments for bash: --login to get environment, -i for interactive mode
            #[cfg(unix)]
            if shell_cmd.contains("bash") {
                cmd.arg("--login");
                cmd.arg("-i");
            }
        }

        // Spawn the child process and store it for cleanup
        let child = pair
            .slave
            .spawn_command(cmd)
            .map_err(|_| "Failed to spawn shell")?;

        // Get reader and writer
        let reader = pair.master.try_clone_reader()
            .map_err(|_| "Failed to clone reader")?;
        let writer = pair.master.take_writer()
            .map_err(|_| "Failed to take writer")?;

        // Wrap in Arc<Mutex<>> for shared access
        let reader_arc = Arc::new(Mutex::new(reader));
        let writer_arc = Arc::new(Mutex::new(writer));

        // Wait for initial prompt
        std::thread::sleep(Duration::from_millis(INITIAL_PROMPT_WAIT_MS));

        let mut output_buffer = Vec::new();
        let mut temp_buf = [0u8; 4096];

        for _ in 0..INITIAL_PROMPT_MAX_RETRIES {
            let (tx, rx) = channel();
            let reader_clone = Arc::clone(&reader_arc);

            std::thread::spawn(move || {
                if let Ok(mut reader) = reader_clone.lock() {
                    let mut buf = [0u8; 4096];
                    match reader.read(&mut buf) {
                        Ok(n) => { let _ = tx.send((n, buf)); }
                        Err(_) => { let _ = tx.send((0, buf)); }
                    }
                }
            });

            match rx.recv_timeout(Duration::from_millis(ITERATION_SLEEP_MS)) {
                Ok((n, buf)) if n > 0 => {
                    output_buffer.extend_from_slice(&buf[..n]);
                    let output_str = String::from_utf8_lossy(&output_buffer);
                    if is_prompt(&output_str) {
                        break;
                    }
                }
                _ => {}
            }
            std::thread::sleep(Duration::from_millis(ITERATION_SLEEP_MS));
        }

        shlog_trace!("Shell ready");
        shlog_trace!("Local shell created successfully");

        // Create shared output buffer and state
        let output_buffer = Arc::new(Mutex::new(Vec::new()));
        let is_alive = Arc::new(AtomicBool::new(true));
        let state = Arc::new(Mutex::new(SessionState::Running));

        // Start reader thread
        let reader_clone = Arc::clone(&reader_arc);
        let buffer_clone = Arc::clone(&output_buffer);
        let alive_clone = Arc::clone(&is_alive);

        let reader_thread = std::thread::spawn(move || {
            shlog_trace!("Reader thread started");
            let mut buf = [0u8; 4096];

            loop {
                // Check if we should exit (lock-free atomic check)
                if !alive_clone.load(Ordering::Acquire) {
                    shlog_trace!("Reader thread exiting");
                    break;
                }

                // Read from PTY (this will block until data is available)
                // The is_alive check above ensures we exit gracefully when session is closed
                match reader_clone.lock() {
                    Ok(mut reader) => {
                        match reader.read(&mut buf) {
                            Ok(n) if n > 0 => {
                                // Append to shared buffer with truncation
                                match buffer_clone.lock() {
                                    Ok(mut buffer) => {
                                        buffer.extend_from_slice(&buf[..n]);

                                        // Truncate buffer if it exceeds max size
                                        if buffer.len() > MAX_BUFFER_BYTES {
                                            if truncate_to_tail(&mut buffer, MAX_BUFFER_BYTES) {
                                                shlog_trace!("Reader thread: buffer truncated to {} bytes", buffer.len());
                                            }
                                        }

                                        shlog_trace!("Reader thread: read {} bytes, buffer now {} bytes", n, buffer.len());
                                    }
                                    Err(e) => {
                                        shlog_trace!("Reader thread: buffer lock poisoned: {}", e);
                                        break;
                                    }
                                }
                            }
                            Ok(_) => {
                                // EOF - process died
                                alive_clone.store(false, Ordering::Release);
                                shlog_trace!("Reader thread: EOF detected, process died");
                                break;
                            }
                            Err(e) => {
                                // Read error - process died or FD closed
                                alive_clone.store(false, Ordering::Release);
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
        });

        // Create LocalShellSession object
        let local_shell = LocalShellSession {
            pair: Arc::new(Mutex::new(Some(pair))),
            reader: reader_arc,
            writer: writer_arc,
            pending_interactive: Arc::new(Mutex::new(None)),
            is_alive,
            state,
            output_buffer,
            reader_thread: Arc::new(Mutex::new(Some(reader_thread))),
            child: Arc::new(Mutex::new(Some(child))),
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

    // Timeout for command execution in seconds
    // The implementation uses 10 iterations per second (ITERATION_SLEEP_MS = 100ms)
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

        // Get timeout parameter and calculate max iterations
        // Each iteration is ITERATION_SLEEP_MS (100ms), so timeout_secs * 10 = max iterations
        let timeout_secs: i64 = self.timeout_secs.get().as_ref().try_into()?;
        let timeout_secs = if timeout_secs <= 0 { 30 } else { timeout_secs };  // Default to 30s if invalid
        let max_iterations = (timeout_secs as usize) * 10;  // 10 iterations per second

        // Extract local shell object
        let local_shell = unsafe {
            Var::from_ref_counted_object::<LocalShellSession>(&session_var, &*LOCAL_SHELL_TYPE)?
        };
        let local_shell = unsafe { &*(local_shell as *const LocalShellSession) };

        // Check if shell is still alive
        if !local_shell.is_alive.load(Ordering::Acquire) {
            // Check state to provide better error message
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
                // State lock is poisoned too
                return Err("Local shell session corrupted (state lock poisoned)");
            }
        }

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

                // Send Ctrl+C to cancel the interactive command
                // Writer lock is released immediately after write to avoid blocking
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
                } // Writer lock released here

                // Give the shell time to process Ctrl+C
                // Any response will be read by the reader thread into the shared buffer
                std::thread::sleep(Duration::from_millis(300));

                // Note: We don't need to drain the response - the shared buffer will be
                // cleared before the next command anyway, and the reader thread handles all reads

                *pending = None;
            }
        }

        // Clear shared buffer before sending command
        {
            let mut shared_buffer = match local_shell.output_buffer.lock() {
                Ok(guard) => guard,
                Err(_) => {
                    mark_session_corrupted(local_shell, "buffer lock poisoned before command");
                    return Err("Buffer lock poisoned");
                }
            };
            shared_buffer.clear();
            shlog_trace!("Cleared shared buffer before command");
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

        // Read output from shared buffer with timeout-based prompt detection
        let mut output_buffer = Vec::new();
        let mut last_buffer_size = 0;
        let mut no_data_count = 0;
        let mut prompt_detected = false;
        let mut was_truncated = false;

        shlog_trace!("Starting to read command output from shared buffer");

        // Give shell time to process command
        std::thread::sleep(Duration::from_millis(COMMAND_OUTPUT_WAIT_MS));

        for iteration in 0..max_iterations {
            // Read from shared buffer atomically (single lock acquisition to avoid race conditions)
            let new_data = {
                let shared_buffer = local_shell.output_buffer.lock()
                    .map_err(|_| "Buffer lock poisoned")?;

                // Only copy new data since last read
                if shared_buffer.len() > last_buffer_size {
                    shared_buffer[last_buffer_size..].to_vec()
                } else {
                    Vec::new()
                }
            };

            let bytes_read = new_data.len();

            if bytes_read > 0 {
                // Append new data to our local output buffer
                output_buffer.extend_from_slice(&new_data);
                last_buffer_size += bytes_read;

                // Truncate if buffer exceeds max size
                if truncate_to_tail(&mut output_buffer, max_output_bytes) {
                    was_truncated = true;
                    shlog_trace!("Output buffer truncated to tail ({} bytes)", max_output_bytes);
                }

                let output_str = String::from_utf8_lossy(&output_buffer);

                shlog_trace!(
                    "Read {} bytes (iteration {}), buffer size: {}, last line: {:?}",
                    bytes_read,
                    iteration,
                    output_buffer.len(),
                    output_str.lines().last()
                );

                // Check for prompt
                if is_prompt(&output_str) {
                    prompt_detected = true;
                    shlog_trace!("Prompt detected in output, command completed");
                    break;
                }
                no_data_count = 0;
            } else {
                no_data_count += 1;
                shlog_trace!(
                    "No data (iteration {}), no_data_count: {}, buffer_empty: {}",
                    iteration,
                    no_data_count,
                    output_buffer.is_empty()
                );

                // Interactive command detection logic:
                // We detect a command as interactive if it stops producing output but doesn't return to shell prompt.
                //
                // Conditions explained:
                // 1. no_data_count >= INTERACTIVE_DETECTION_ITERATIONS (30 iterations = 3 seconds):
                //    - Gives command enough time to complete output before assuming it's waiting for input
                //    - 3 seconds balances between false positives (slow commands) and responsiveness
                //
                // 2. !output_buffer.is_empty():
                //    - Command must have produced SOME output (avoids detecting hung commands as interactive)
                //    - Interactive prompts typically display text before waiting
                //
                // 3. iteration >= INTERACTIVE_DETECTION_ITERATIONS / 2 (≥15 iterations = ≥1.5 seconds):
                //    - Prevents premature detection during command startup
                //    - Allows time for fast commands to complete normally
                //
                if no_data_count >= INTERACTIVE_DETECTION_ITERATIONS && !output_buffer.is_empty() && iteration >= INTERACTIVE_DETECTION_ITERATIONS / 2 {
                    let output_str = String::from_utf8_lossy(&output_buffer);

                    // First check for shell prompt (command completed normally)
                    if is_prompt(&output_str) {
                        prompt_detected = true;
                        shlog_trace!("Prompt detected on final check");
                        break;
                    }

                    // Check for common interactive prompt patterns (password, confirmation, etc.)
                    if is_interactive_prompt(&output_str) {
                        shlog_trace!("Interactive prompt pattern detected: {:?}", output_str.lines().last());
                        break;
                    }

                    // No shell prompt, no obvious interactive pattern, but no new data for 2 seconds
                    // Likely an interactive command waiting for input
                    shlog_trace!("Command appears to be interactive (no new data for 2s, no prompt detected)");
                    break;
                }
            }
            std::thread::sleep(Duration::from_millis(ITERATION_SLEEP_MS));
        }

        // Determine status and prepare output
        let mut result_table = AutoTableVar::new();
        let output_str = String::from_utf8_lossy(&output_buffer).to_string();

        if prompt_detected {
            // Command completed normally
            let final_output = if should_clean {
                clean_output(&output_str)
            } else {
                output_str
            };

            result_table.0.insert_fast_static("status", &Var::ephemeral_string("completed"));
            result_table.0.insert_fast_static("output", &Var::ephemeral_string(&final_output));
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
                clean_output(&output_str)
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

        self.output = result_table.to_cloned();
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

    // Timeout for reading response after sending input, in seconds
    // The implementation uses 10 iterations per second (ITERATION_SLEEP_MS = 100ms)
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

        // Get timeout parameter and calculate max iterations
        let timeout_secs: i64 = self.timeout_secs.get().as_ref().try_into()?;
        let timeout_secs = if timeout_secs <= 0 { 10 } else { timeout_secs };  // Default to 10s if invalid
        let max_iterations = (timeout_secs as usize) * 10;  // 10 iterations per second

        // Extract local shell object
        let local_shell = unsafe {
            Var::from_ref_counted_object::<LocalShellSession>(&session_var, &*LOCAL_SHELL_TYPE)?
        };
        let local_shell = unsafe { &*(local_shell as *const LocalShellSession) };

        // Check if shell is still alive
        if !local_shell.is_alive.load(Ordering::Acquire) {
            // Check state to provide better error message
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
                // State lock is poisoned too
                return Err("Local shell session corrupted (state lock poisoned)");
            }
        }

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

        // Note current buffer position before sending input
        let start_buffer_size = {
            let shared_buffer = match local_shell.output_buffer.lock() {
                Ok(guard) => guard,
                Err(_) => {
                    mark_session_corrupted(local_shell, "buffer lock poisoned in SendInput");
                    return Err("Buffer lock poisoned");
                }
            };
            shared_buffer.len()
        };

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

        // Wait for output - give shell time to process input
        std::thread::sleep(Duration::from_millis(SENDINPUT_INITIAL_WAIT_MS));
        shlog_trace!("SendInput: Starting to read output after input (starting from byte {})", start_buffer_size);

        // Read output from shared buffer
        let mut output_buffer = Vec::new();
        let mut last_buffer_size = start_buffer_size;
        let mut was_truncated = false;
        let mut no_data_count = 0;

        for iteration in 0..max_iterations {
            // Read from shared buffer atomically (single lock acquisition to avoid race conditions)
            let new_data = {
                let shared_buffer = local_shell.output_buffer.lock()
                    .map_err(|_| "Buffer lock poisoned")?;

                // Only copy new data since last read
                if shared_buffer.len() > last_buffer_size {
                    shared_buffer[last_buffer_size..].to_vec()
                } else {
                    Vec::new()
                }
            };

            let bytes_read = new_data.len();

            if bytes_read > 0 {
                // Append new data to our local output buffer
                output_buffer.extend_from_slice(&new_data);
                last_buffer_size += bytes_read;

                // Truncate if buffer exceeds max size
                if truncate_to_tail(&mut output_buffer, max_output_bytes) {
                    was_truncated = true;
                    shlog_trace!("Output buffer truncated to tail ({} bytes)", max_output_bytes);
                }

                let output_str = String::from_utf8_lossy(&output_buffer);
                shlog_trace!(
                    "SendInput: Read {} bytes (iteration {}), buffer size: {}, content: {:?}",
                    bytes_read,
                    iteration,
                    output_buffer.len(),
                    output_str
                );

                // Reset no-data counter
                no_data_count = 0;

                // Check if we got a prompt in the new data - if so, we're done
                if is_prompt(&output_str) {
                    shlog_trace!("SendInput: Prompt detected in new output, exiting early");
                    break;
                }
            } else {
                no_data_count += 1;
                shlog_trace!("SendInput: No new data (iteration {}), buffer_size: {}, no_data_count: {}",
                    iteration, output_buffer.len(), no_data_count);

                // Early exit: if no data for a while, check buffer range for prompt
                // Check from start_buffer_size to last_buffer_size (only NEW data)
                // This avoids detecting prompts from before SendInput was called
                if no_data_count >= SENDINPUT_NO_DATA_THRESHOLD {
                    if check_buffer_for_prompt(local_shell, start_buffer_size, last_buffer_size)? {
                        shlog_trace!("SendInput: No new data for {}s and prompt detected in buffer range [{}..{}], exiting early",
                            (SENDINPUT_NO_DATA_THRESHOLD as u64 * ITERATION_SLEEP_MS) / 1000,
                            start_buffer_size,
                            last_buffer_size);
                        break;
                    }
                }
            }
            std::thread::sleep(Duration::from_millis(ITERATION_SLEEP_MS));
        }

        // Capture final buffer position to avoid race condition with reader thread
        let final_buffer_size = last_buffer_size;
        shlog_trace!("SendInput: Finished reading, total output: {} bytes, final buffer position: {}",
            output_buffer.len(), final_buffer_size);

        // Check for prompt in the new output
        let output_str = String::from_utf8_lossy(&output_buffer).to_string();
        let mut prompt_detected = is_prompt(&output_str);

        // If no prompt detected in new output, check the shared buffer range
        // This handles the case where the command completed and the prompt was written
        // to the buffer before the reading loop could see it. We check from start_buffer_size
        // to final_buffer_size to only examine NEW data, avoiding race conditions
        if !prompt_detected {
            prompt_detected = check_buffer_for_prompt(local_shell, start_buffer_size, final_buffer_size)?;
            shlog_trace!("SendInput: Checked buffer range [{}..{}] for prompt, detected: {}",
                start_buffer_size, final_buffer_size, prompt_detected);
        }

        // Clean output
        let stripped_bytes = strip_ansi_escapes::strip(&output_str);
        let cleaned = String::from_utf8_lossy(&stripped_bytes);
        let mut lines: Vec<&str> = cleaned.lines().collect();
        if prompt_detected && !lines.is_empty() {
            lines.pop(); // Remove prompt line
        }
        let final_output = lines.join("\n");

        let mut result_table = AutoTableVar::new();

        if prompt_detected {
            // Interactive session completed
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
            // Still waiting for more input/output
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
        // Extract local shell object
        let local_shell =
            unsafe { Var::from_ref_counted_object::<LocalShellSession>(&input, &*LOCAL_SHELL_TYPE)? };
        let local_shell = unsafe { &*(local_shell as *const LocalShellSession) };

        // Check alive status flag (lock-free atomic read)
        let is_alive = local_shell.is_alive.load(Ordering::Acquire);

        self.output = is_alive.into();
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

    // Register LocalShellSession object type
    let mut info = shards::SHObjectInfo::default();
    info.name = cstr!("LocalShell.Session").as_ptr() as shards::SHString;
    shards::core::register_object_type_internal(FRAG_CC, fourCharacterCode(*b"lshl"), info);

    // Register shards
    register_shard::<CreateShard>();
    register_shard::<ExecuteShard>();
    register_shard::<SendInputShard>();
    register_shard::<IsAliveShard>();

    shlog_trace!("LocalShell module registered");
}
