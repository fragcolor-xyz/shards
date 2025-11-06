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

use portable_pty::{CommandBuilder, PtySize, PtySystem};
use std::io::{Read, Write};
use std::sync::{Arc, Mutex};
use std::sync::mpsc::{channel, Receiver, TryRecvError};
use std::time::Duration;

// LocalShell session object wrapper
mod local_shell {
    use super::*;

    pub struct LocalShellSession {
        pub pair: Arc<Mutex<portable_pty::PtyPair>>,
        pub reader: Arc<Mutex<Box<dyn Read + Send>>>,
        pub writer: Arc<Mutex<Box<dyn Write + Send>>>,
        pub pending_interactive: Arc<Mutex<Option<InteractiveState>>>,
        pub is_alive: Arc<Mutex<bool>>,
        pub output_buffer: Arc<Mutex<Vec<u8>>>,
        pub reader_thread: Arc<Mutex<Option<std::thread::JoinHandle<()>>>>,
    }

    #[derive(Clone)]
    pub struct InteractiveState {
        pub original_cmd: String,
    }

    impl Drop for LocalShellSession {
        fn drop(&mut self) {
            shlog_trace!("Dropping LocalShellSession, cleaning up");

            // Mark as not alive (this will signal the reader thread to exit)
            if let Ok(mut is_alive) = self.is_alive.lock() {
                *is_alive = false;
            }

            // Wait for reader thread to finish
            if let Ok(mut thread_opt) = self.reader_thread.lock() {
                if let Some(thread) = thread_opt.take() {
                    shlog_trace!("Waiting for reader thread to finish");
                    let _ = thread.join();
                }
            }

            // Close writer to signal shell to exit
            if let Ok(mut writer) = self.writer.lock() {
                let _ = writer.flush();
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

    output: ClonedVar,
}

impl Default for CreateShard {
    fn default() -> Self {
        Self {
            required: ExposedTypes::new(),
            shell: ParamVar::new(Var::default()),
            working_dir: ParamVar::new(Var::default()),
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

        // For bash, use --login to get environment and -i for interactive mode
        #[cfg(unix)]
        if shell_cmd.contains("bash") {
            cmd.arg("--login");
            cmd.arg("-i");
        }

        // Spawn the child process
        let _child = pair
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
        std::thread::sleep(Duration::from_millis(500));

        let mut output_buffer = Vec::new();
        let mut temp_buf = [0u8; 4096];

        for _ in 0..10 {
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

            match rx.recv_timeout(Duration::from_millis(100)) {
                Ok((n, buf)) if n > 0 => {
                    output_buffer.extend_from_slice(&buf[..n]);
                    let output_str = String::from_utf8_lossy(&output_buffer);
                    if is_prompt(&output_str) {
                        break;
                    }
                }
                _ => {}
            }
            std::thread::sleep(Duration::from_millis(100));
        }

        shlog_trace!("Shell ready");
        shlog_trace!("Local shell created successfully");

        // Create shared output buffer
        let output_buffer = Arc::new(Mutex::new(Vec::new()));
        let is_alive = Arc::new(Mutex::new(true));

        // Start reader thread
        let reader_clone = Arc::clone(&reader_arc);
        let buffer_clone = Arc::clone(&output_buffer);
        let alive_clone = Arc::clone(&is_alive);

        let reader_thread = std::thread::spawn(move || {
            shlog_trace!("Reader thread started");
            let mut buf = [0u8; 4096];

            loop {
                // Check if we should exit (non-blocking check)
                match alive_clone.lock() {
                    Ok(alive) if !*alive => {
                        shlog_trace!("Reader thread exiting");
                        break;
                    }
                    _ => {}
                }

                // Read from PTY (this will block until data is available)
                if let Ok(mut reader) = reader_clone.lock() {
                    match reader.read(&mut buf) {
                        Ok(n) if n > 0 => {
                            // Append to shared buffer
                            if let Ok(mut buffer) = buffer_clone.lock() {
                                buffer.extend_from_slice(&buf[..n]);
                                shlog_trace!("Reader thread: read {} bytes, buffer now {} bytes", n, buffer.len());
                            }
                        }
                        Ok(_) => {
                            // EOF - process died
                            if let Ok(mut alive) = alive_clone.lock() {
                                *alive = false;
                            }
                            break;
                        }
                        Err(e) => {
                            shlog_trace!("Reader thread: read error: {}", e);
                            if let Ok(mut alive) = alive_clone.lock() {
                                *alive = false;
                            }
                            break;
                        }
                    }
                } else {
                    // Failed to lock reader
                    break;
                }
            }
            shlog_trace!("Reader thread finished");
        });

        // Create LocalShellSession object
        let local_shell = LocalShellSession {
            pair: Arc::new(Mutex::new(pair)),
            reader: reader_arc,
            writer: writer_arc,
            pending_interactive: Arc::new(Mutex::new(None)),
            is_alive,
            output_buffer,
            reader_thread: Arc::new(Mutex::new(Some(reader_thread))),
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

        // Extract local shell object
        let local_shell = unsafe {
            Var::from_ref_counted_object::<LocalShellSession>(&session_var, &*LOCAL_SHELL_TYPE)?
        };
        let local_shell = unsafe { &*(local_shell as *const LocalShellSession) };

        // Check if shell is still alive
        {
            let is_alive = local_shell.is_alive.lock()
                .map_err(|_| "Alive state lock poisoned")?;
            if !*is_alive {
                return Err("Local shell process has exited");
            }
        }

        // Check if there's a pending interactive command
        {
            let mut pending = local_shell.pending_interactive.lock()
                .map_err(|_| "Interactive state lock poisoned")?;
            if pending.is_some() {
                shlog_trace!("New command received while interactive command was pending, sending Ctrl+C");

                // Send Ctrl+C to cancel the interactive command
                {
                    let mut writer = local_shell.writer.lock()
                        .map_err(|_| "Writer lock poisoned")?;
                    let _ = writer.write_all(&[3]);
                    let _ = writer.flush();
                }

                // Give the shell time to process Ctrl+C
                std::thread::sleep(Duration::from_millis(300));

                // Drain any Ctrl+C response
                {
                    let mut reader = local_shell.reader.lock()
                        .map_err(|_| "Reader lock poisoned")?;
                    let mut drain_buf = [0u8; 4096];
                    for _ in 0..5 {
                        match reader.read(&mut drain_buf) {
                            Ok(n) if n > 0 => {
                                shlog_trace!("Drained {} bytes after Ctrl+C", n);
                            }
                            _ => break,
                        }
                        std::thread::sleep(Duration::from_millis(100));
                    }
                }

                *pending = None;
            }
        }

        // Clear shared buffer before sending command
        {
            let mut shared_buffer = local_shell.output_buffer.lock()
                .map_err(|_| "Buffer lock poisoned")?;
            shared_buffer.clear();
            shlog_trace!("Cleared shared buffer before command");
        }

        // Send command
        let cmd_with_newline = format!("{}\n", cmd);
        {
            let mut writer = local_shell.writer.lock()
                .map_err(|_| "Writer lock poisoned")?;
            writer.write_all(cmd_with_newline.as_bytes())
                .map_err(|_| "Failed to write command")?;
            writer.flush()
                .map_err(|_| "Failed to flush writer")?;
        }

        // Read output from shared buffer with timeout-based prompt detection
        let mut output_buffer = Vec::new();
        let mut last_buffer_size = 0;
        let mut no_data_count = 0;
        let max_iterations = 50; // 5 seconds total (50 * 100ms)
        let mut prompt_detected = false;
        let mut was_truncated = false;

        shlog_trace!("Starting to read command output from shared buffer");

        // Give shell time to process command
        std::thread::sleep(Duration::from_millis(200));

        for iteration in 0..max_iterations {
            // Read from shared buffer
            let current_buffer_size = {
                let shared_buffer = local_shell.output_buffer.lock()
                    .map_err(|_| "Buffer lock poisoned")?;
                shared_buffer.len()
            };

            let bytes_read = current_buffer_size - last_buffer_size;

            if bytes_read > 0 {
                // Copy new data from shared buffer
                let shared_buffer = local_shell.output_buffer.lock()
                    .map_err(|_| "Buffer lock poisoned")?;
                output_buffer.extend_from_slice(&shared_buffer[last_buffer_size..]);
                drop(shared_buffer);

                last_buffer_size = current_buffer_size;

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

                // Only consider interactive if we have output AND consistent no-data period
                if no_data_count >= 20 && !output_buffer.is_empty() && iteration >= 10 {
                    // Check one more time if there's a prompt we might have missed
                    let output_str = String::from_utf8_lossy(&output_buffer);
                    if is_prompt(&output_str) {
                        prompt_detected = true;
                        shlog_trace!("Prompt detected on final check");
                        break;
                    }
                    // No output for 2 seconds after having received some data, likely interactive
                    shlog_trace!("Command appears to be interactive (no new data for 2s)");
                    break;
                }
            }
            std::thread::sleep(Duration::from_millis(100));
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

        // Extract local shell object
        let local_shell = unsafe {
            Var::from_ref_counted_object::<LocalShellSession>(&session_var, &*LOCAL_SHELL_TYPE)?
        };
        let local_shell = unsafe { &*(local_shell as *const LocalShellSession) };

        // Check if shell is still alive
        {
            let is_alive = local_shell.is_alive.lock()
                .map_err(|_| "Alive state lock poisoned")?;
            if !*is_alive {
                return Err("Local shell process has exited");
            }
        }

        // Check if there's a pending interactive command
        {
            let pending = local_shell.pending_interactive.lock()
                .map_err(|_| "Interactive state lock poisoned")?;
            if pending.is_none() {
                return Err("No interactive command is pending");
            }
        }

        // Note current buffer position before sending input
        let start_buffer_size = {
            let shared_buffer = local_shell.output_buffer.lock()
                .map_err(|_| "Buffer lock poisoned")?;
            shared_buffer.len()
        };

        // Send input (if not empty)
        if !input_str.is_empty() {
            let input_with_newline = format!("{}\n", input_str);
            shlog_trace!("SendInput: Writing {} bytes: {:?}", input_with_newline.len(), input_with_newline);
            let mut writer = local_shell.writer.lock()
                .map_err(|_| "Writer lock poisoned")?;
            writer.write_all(input_with_newline.as_bytes())
                .map_err(|_| "Failed to write input")?;
            writer.flush()
                .map_err(|_| "Failed to flush writer")?;
            shlog_trace!("SendInput: Write and flush succeeded");
        }

        // Wait for output - give shell time to process input
        std::thread::sleep(Duration::from_millis(500));
        shlog_trace!("SendInput: Starting to read output after input (starting from byte {})", start_buffer_size);

        // Read output from shared buffer
        let mut output_buffer = Vec::new();
        let mut last_buffer_size = start_buffer_size;
        let mut was_truncated = false;

        for iteration in 0..30 {  // 3 seconds total (30 * 100ms)
            // Read from shared buffer
            let current_buffer_size = {
                let shared_buffer = local_shell.output_buffer.lock()
                    .map_err(|_| "Buffer lock poisoned")?;
                shared_buffer.len()
            };

            let bytes_read = current_buffer_size - last_buffer_size;

            if bytes_read > 0 {
                // Copy new data from shared buffer
                let shared_buffer = local_shell.output_buffer.lock()
                    .map_err(|_| "Buffer lock poisoned")?;
                output_buffer.extend_from_slice(&shared_buffer[last_buffer_size..]);
                drop(shared_buffer);

                last_buffer_size = current_buffer_size;

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
            } else {
                shlog_trace!("SendInput: No new data (iteration {}), buffer_size: {}", iteration, output_buffer.len());
            }
            std::thread::sleep(Duration::from_millis(100));
        }

        shlog_trace!("SendInput: Finished reading, total output: {} bytes", output_buffer.len());

        // Check for prompt
        let output_str = String::from_utf8_lossy(&output_buffer).to_string();
        let prompt_detected = is_prompt(&output_str);

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

        // Check alive status flag
        let is_alive = local_shell.is_alive.lock()
            .map_err(|_| "Alive state lock poisoned")?;

        self.output = (*is_alive).into();
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
