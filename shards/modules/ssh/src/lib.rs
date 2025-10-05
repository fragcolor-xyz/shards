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
use shards::types::ExposedInfo;
use shards::types::ExposedTypes;
use shards::types::InstanceData;
use shards::types::OptionalString;
use shards::types::ParamVar;
use shards::types::Parameters;
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
use std::time::Duration;

// SSH Shell object wrapper
mod ssh_shell {
    use super::*;

    pub struct SSHShell {
        pub session: Arc<Mutex<Session>>,
        pub channel: Arc<Mutex<ssh2::Channel>>,
        pub pending_interactive: Arc<Mutex<Option<InteractiveState>>>,
        pub is_connected: Arc<Mutex<bool>>,
    }

    #[derive(Clone)]
    pub struct InteractiveState {
        pub original_cmd: String,
    }

    impl Drop for SSHShell {
        fn drop(&mut self) {
            shlog_trace!("Dropping SSHShell, cleaning up connection");

            // Mark as disconnected
            if let Ok(mut is_connected) = self.is_connected.lock() {
                *is_connected = false;
            }

            // Close channel gracefully
            if let Ok(mut channel) = self.channel.lock() {
                let _ = channel.send_eof();
                let _ = channel.wait_eof();
                let _ = channel.close();
                let _ = channel.wait_close();
            }

            // Disconnect session
            if let Ok(session) = self.session.lock() {
                let _ = session.disconnect(None, "Session closed", None);
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

// Helper function to detect shell prompts (NOT interactive program prompts)
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

// Helper function to clean output (remove ANSI codes, command echo, prompts)
fn clean_output(output: &str, cmd: &str) -> String {
    // Strip ANSI escape sequences
    let stripped_bytes = strip_ansi_escapes::strip(output);
    let text = String::from_utf8_lossy(&stripped_bytes);
    let mut lines: Vec<&str> = text.lines().collect();

    // Remove command echo (first line if it matches the command)
    if lines.first().map_or(false, |l| l.trim() == cmd.trim() || l.contains(cmd)) {
        lines.remove(0);
    }

    // Remove trailing prompt if present
    if !lines.is_empty() {
        let last_line = lines.last().unwrap();
        if is_prompt(last_line) {
            lines.pop();
        }
    }

    // Join lines and remove empty lines at start/end
    let result = lines.join("\n");
    result.trim().to_string()
}

// Helper function to check if an IO error indicates connection loss
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

        // Connect to SSH server
        let addr = format!("{}:{}", host, port);

        // Resolve hostname to socket address
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

        // Open channel and request PTY
        let mut channel = sess.channel_session().map_err(|_| "Failed to open channel")?;
        channel
            .request_pty("xterm", None, None)
            .map_err(|_| "Failed to request PTY")?;
        channel.shell().map_err(|_| "Failed to start shell")?;

        // Set non-blocking mode for timeout-based reads
        sess.set_blocking(false);

        // Drain initial prompt (wait for shell to be ready)
        std::thread::sleep(Duration::from_millis(500));
        let mut output_buffer = Vec::new();
        let mut temp_buf = [0u8; 4096];
        for _ in 0..10 {
            match channel.read(&mut temp_buf) {
                Ok(n) if n > 0 => {
                    output_buffer.extend_from_slice(&temp_buf[..n]);
                    let output_str = String::from_utf8_lossy(&output_buffer);
                    if is_prompt(&output_str) {
                        break;
                    }
                }
                _ => {}
            }
            std::thread::sleep(Duration::from_millis(100));
        }

        shlog_trace!("SSH connection established, shell ready");

        // Create SSHShell object
        let ssh_shell = SSHShell {
            session: Arc::new(Mutex::new(sess)),
            channel: Arc::new(Mutex::new(channel)),
            pending_interactive: Arc::new(Mutex::new(None)),
            is_connected: Arc::new(Mutex::new(true)),
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

    output: ClonedVar,
}

impl Default for ExecuteShard {
    fn default() -> Self {
        Self {
            required: ExposedTypes::new(),
            session: ParamVar::default(),
            timeout_secs: ParamVar::new(30i64.into()),
            clean_output: ParamVar::new(true.into()),
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

        // Extract SSH shell object
        let ssh_shell = unsafe {
            Var::from_ref_counted_object::<SSHShell>(&session_var, &*SSH_SHELL_TYPE)?
        };
        let ssh_shell = unsafe { &*(ssh_shell as *const SSHShell) };

        // Check if connection is still alive
        {
            let is_connected = ssh_shell.is_connected.lock()
                .map_err(|_| "Connection state lock poisoned")?;
            if !*is_connected {
                return Err("SSH connection lost");
            }
        }

        // Check if there's a pending interactive command
        {
            let mut pending = ssh_shell.pending_interactive.lock()
                .map_err(|_| "Interactive state lock poisoned")?;
            if pending.is_some() {
                shlog_trace!("New command received while interactive command was pending, sending Ctrl+C");
                // Send Ctrl+C to cancel
                let mut channel = ssh_shell.channel.lock()
                    .map_err(|_| "SSH channel lock poisoned")?;
                let _ = channel.write_all(&[3]);
                std::thread::sleep(Duration::from_millis(100));
                *pending = None;
            }
        }

        // Send command
        let cmd_with_newline = format!("{}\n", cmd);
        {
            let mut channel = ssh_shell.channel.lock()
                .map_err(|_| "SSH channel lock poisoned")?;
            if let Err(e) = channel.write_all(cmd_with_newline.as_bytes()) {
                if is_connection_error(&e) {
                    *ssh_shell.is_connected.lock()
                        .map_err(|_| "Connection state lock poisoned")? = false;
                    shlog_trace!("Connection lost during write: {:?}", e);
                    return Err("SSH connection lost");
                }
                return Err("Failed to send command");
            }
        }

        // Read output with timeout-based prompt detection
        let mut output_buffer = Vec::new();
        let mut temp_buf = [0u8; 4096];
        let mut no_data_count = 0;
        let max_iterations = 50; // 5 seconds total (50 * 100ms)
        let mut prompt_detected = false;

        shlog_trace!("Starting to read command output");

        // Give shell time to process and echo the command
        std::thread::sleep(Duration::from_millis(200));

        for iteration in 0..max_iterations {
            let read_result = {
                let mut channel = ssh_shell.channel.lock()
                    .map_err(|_| "SSH channel lock poisoned")?;
                channel.read(&mut temp_buf)
            };

            let bytes_read = match read_result {
                Ok(n) => n,
                Err(e) if e.kind() == std::io::ErrorKind::WouldBlock => 0,
                Err(e) if is_connection_error(&e) => {
                    *ssh_shell.is_connected.lock()
                        .map_err(|_| "Connection state lock poisoned")? = false;
                    shlog_trace!("Connection lost during read: {:?}", e);

                    // Return connection_lost status instead of error
                    let mut result_table = AutoTableVar::new();
                    result_table.0.insert_fast_static("status", &Var::ephemeral_string("connection_lost"));
                    result_table.0.insert_fast_static("output", &Var::ephemeral_string(""));
                    result_table.0.insert_fast_static("message", &Var::ephemeral_string("SSH connection lost"));
                    self.output = result_table.to_cloned();
                    return Ok(self.output.0);
                }
                Err(_) => 0, // Other errors treated as no data
            };

            if bytes_read > 0 {
                output_buffer.extend_from_slice(&temp_buf[..bytes_read]);
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
                no_data_count = 0; // Reset no-data count on new data
            } else {
                no_data_count += 1;
                shlog_trace!(
                    "No data (iteration {}), no_data_count: {}, buffer_empty: {}",
                    iteration,
                    no_data_count,
                    output_buffer.is_empty()
                );

                // Only consider interactive if we have output AND consistent no-data period
                // AND we're past the minimum wait time
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
                clean_output(&output_str, cmd)
            } else {
                output_str
            };

            result_table.0.insert_fast_static("status", &Var::ephemeral_string("completed"));
            result_table.0.insert_fast_static("output", &Var::ephemeral_string(&final_output));
        } else {
            // Command is interactive (waiting for input)
            {
                let mut pending = ssh_shell.pending_interactive.lock()
                    .map_err(|_| "Interactive state lock poisoned")?;
                *pending = Some(InteractiveState {
                    original_cmd: cmd.to_string(),
                });
            }

            let partial_output = if should_clean {
                clean_output(&output_str, cmd)
            } else {
                output_str
            };

            result_table.0.insert_fast_static("status", &Var::ephemeral_string("requires_interaction"));
            result_table.0.insert_fast_static("output", &Var::ephemeral_string(&partial_output));
            result_table.0.insert_fast_static(
                "message",
                &Var::ephemeral_string("Command is waiting for input. Use SSH.SendInput to interact.")
            );
        }

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

    output: ClonedVar,
}

impl Default for SendInputShard {
    fn default() -> Self {
        Self {
            required: ExposedTypes::new(),
            session: ParamVar::default(),
            timeout_secs: ParamVar::new(10i64.into()),
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

        // Extract SSH shell object
        let ssh_shell = unsafe {
            Var::from_ref_counted_object::<SSHShell>(&session_var, &*SSH_SHELL_TYPE)?
        };
        let ssh_shell = unsafe { &*(ssh_shell as *const SSHShell) };

        // Check if connection is still alive
        {
            let is_connected = ssh_shell.is_connected.lock()
                .map_err(|_| "Connection state lock poisoned")?;
            if !*is_connected {
                return Err("SSH connection lost");
            }
        }

        // Check if there's a pending interactive command
        {
            let pending = ssh_shell.pending_interactive.lock()
                .map_err(|_| "Interactive state lock poisoned")?;
            if pending.is_none() {
                return Err("No interactive command is pending");
            }
        }

        // Send input (if not empty - empty means just check output)
        if !input_str.is_empty() {
            let input_with_newline = format!("{}\n", input_str);
            let mut channel = ssh_shell.channel.lock()
                .map_err(|_| "SSH channel lock poisoned")?;
            if let Err(e) = channel.write_all(input_with_newline.as_bytes()) {
                if is_connection_error(&e) {
                    *ssh_shell.is_connected.lock()
                        .map_err(|_| "Connection state lock poisoned")? = false;
                    shlog_trace!("Connection lost during write: {:?}", e);
                    return Err("SSH connection lost");
                }
                return Err("Failed to send input");
            }
        }

        // Wait for output
        std::thread::sleep(Duration::from_secs(2));

        // Read output
        let mut output_buffer = Vec::new();
        let mut temp_buf = [0u8; 4096];
        for _ in 0..10 {
            let read_result = {
                let mut channel = ssh_shell.channel.lock()
                    .map_err(|_| "SSH channel lock poisoned")?;
                channel.read(&mut temp_buf)
            };

            let bytes_read = match read_result {
                Ok(n) => n,
                Err(e) if e.kind() == std::io::ErrorKind::WouldBlock => 0,
                Err(e) if is_connection_error(&e) => {
                    *ssh_shell.is_connected.lock()
                        .map_err(|_| "Connection state lock poisoned")? = false;
                    shlog_trace!("Connection lost during read: {:?}", e);

                    // Return connection_lost status
                    let mut result_table = AutoTableVar::new();
                    result_table.0.insert_fast_static("status", &Var::ephemeral_string("connection_lost"));
                    result_table.0.insert_fast_static("output", &Var::ephemeral_string(""));
                    result_table.0.insert_fast_static("message", &Var::ephemeral_string("SSH connection lost"));
                    self.output = result_table.to_cloned();
                    return Ok(self.output.0);
                }
                Err(_) => 0, // Other errors treated as no data
            };

            if bytes_read > 0 {
                output_buffer.extend_from_slice(&temp_buf[..bytes_read]);
            }
            std::thread::sleep(Duration::from_millis(100));
        }

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
                let mut pending = ssh_shell.pending_interactive.lock()
                    .map_err(|_| "Interactive state lock poisoned")?;
                *pending = None;
            }
            shlog_trace!("Interactive session completed");
            result_table.0.insert_fast_static("status", &Var::ephemeral_string("completed"));
            result_table.0.insert_fast_static("output", &Var::ephemeral_string(&final_output));
        } else {
            // Still waiting for more input/output
            result_table.0.insert_fast_static("status", &Var::ephemeral_string("pending_output"));
            result_table.0.insert_fast_static("output", &Var::ephemeral_string(&final_output));
            result_table.0.insert_fast_static(
                "message",
                &Var::ephemeral_string("Command still running. Use SSH.SendInput to continue.")
            );
        }

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
        // Extract SSH shell object
        let ssh_shell =
            unsafe { Var::from_ref_counted_object::<SSHShell>(&input, &*SSH_SHELL_TYPE)? };
        let ssh_shell = unsafe { &*(ssh_shell as *const SSHShell) };

        // Check connection status flag
        // Note: We don't probe the connection here to avoid consuming data from the channel
        // The flag is set to false whenever a connection error is detected in Execute/SendInput
        let is_connected = ssh_shell.is_connected.lock()
            .map_err(|_| "Connection state lock poisoned")?;

        self.output = (*is_connected).into();
        Ok(Some(self.output.0))
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

    shlog_trace!("SSH module registered");
}
