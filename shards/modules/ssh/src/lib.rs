/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2025 Fragcolor Pte. Ltd. */

#![allow(non_upper_case_globals)]
#![allow(non_camel_case_types)]
#![allow(non_snake_case)]

#[macro_use]
extern crate shards;

#[macro_use]
extern crate lazy_static;

use russh::client::{self, Handle, Handler};
use russh::*;
use russh_keys::*;
use shards::core::register_shard;
use shards::core::run_future;
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
use std::sync::{Arc, Mutex};
use std::time::Duration;
use tokio::time::timeout;

// SSH Shell object wrapper
mod ssh_shell {
    use super::*;

    pub struct SSHShell {
        pub session: Handle<ClientHandler>,
        pub channel: ChannelId,
        pub pending_interactive: Arc<Mutex<Option<InteractiveState>>>,
    }

    #[derive(Clone)]
    pub struct InteractiveState {
        pub original_cmd: String,
    }

    // Implement ref-counted object type for SSHShell
    impl Drop for SSHShell {
        fn drop(&mut self) {
            shlog_trace!("Dropping SSHShell");
        }
    }

    ref_counted_object_type_impl!(SSHShell);
}

use ssh_shell::*;

lazy_static! {
    static ref TOKIO_RUNTIME: Arc<Mutex<tokio::runtime::Runtime>> = Arc::new(Mutex::new(
        tokio::runtime::Builder::new_multi_thread()
            .worker_threads(4)
            .enable_all()
            .build()
            .expect("Failed to create Tokio runtime")
    ));
    static ref SSH_SHELL_TYPE: Type = Type::object(FRAG_CC, fourCharacterCode(*b"sshs"));
    static ref SSH_SHELL_TYPE_VEC: Vec<Type> = vec![*SSH_SHELL_TYPE];
    static ref EXECUTE_OUTPUT_TYPES: Vec<Type> = vec![common_type::string_table];
}

// Helper function to detect shell prompts
fn is_prompt(text: &str) -> bool {
    let lines: Vec<&str> = text.lines().collect();
    if let Some(last) = lines.last() {
        let trimmed = last.trim();
        trimmed.ends_with("$ ")
            || trimmed.ends_with("# ")
            || (trimmed.contains(">") && trimmed.ends_with(">"))
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

// Simple SSH client handler
#[derive(Clone)]
struct ClientHandler;

#[async_trait::async_trait]
impl client::Handler for ClientHandler {
    type Error = russh::Error;

    async fn check_server_key(
        &mut self,
        _server_public_key: &key::PublicKey,
    ) -> Result<bool, Self::Error> {
        // Accept any server key (for now - in production, should verify)
        Ok(true)
    }
}

// ============================================================================
// SSH.Connect Shard
// ============================================================================

#[derive(shards::shard)]
#[shard_info("SSH.Connect", "Connect to an SSH server and create a persistent shell session")]
pub struct ConnectShard {
    #[shard_required]
    required: ExposedTypes,

    #[shard_param("Host", "SSH server hostname or IP address", [common_type::string])]
    host: ParamVar,

    #[shard_param("Port", "SSH server port (default: 22)", [common_type::int])]
    port: ParamVar,

    #[shard_param("User", "SSH username", [common_type::string])]
    user: ParamVar,

    #[shard_param("KeyPath", "Path to SSH private key file", [common_type::string, common_type::none])]
    key_path: ParamVar,

    #[shard_param("Password", "SSH password (if not using key)", [common_type::string, common_type::none])]
    password: ParamVar,

    #[shard_param("Timeout", "Connection timeout in seconds (default: 10)", [common_type::int])]
    timeout_secs: ParamVar,

    output: ClonedVar,
}

impl Default for ConnectShard {
    fn default() -> Self {
        Self {
            required: ExposedTypes::new(),
            host: ParamVar::new("localhost".into()),
            port: ParamVar::new(22i64.into()),
            user: ParamVar::new("user".into()),
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

    fn activate(&mut self, context: &Context, _input: &Var) -> Result<Option<Var>, &str> {
        let host: &str = self.host.get().as_ref().try_into()?;
        let port: i64 = self.port.get().as_ref().try_into()?;
        let user: &str = self.user.get().as_ref().try_into()?;
        let timeout_secs: i64 = self.timeout_secs.get().as_ref().try_into()?;

        let key_path_var = self.key_path.get();
        let password_var = self.password.get();

        let host = host.to_string();
        let user = user.to_string();
        let key_path = if !key_path_var.is_none() {
            Some(key_path_var.as_ref().try_into().map(|s: &str| s.to_string())?)
        } else {
            None
        };
        let password = if !password_var.is_none() {
            Some(password_var.as_ref().try_into().map(|s: &str| s.to_string())?)
        } else {
            None
        };

        let result = run_future(
            context,
            async move {
                let runtime = TOKIO_RUNTIME.clone();
                let task: tokio::task::JoinHandle<Result<ClonedVar, String>> = {
                    let runtime = runtime.lock().unwrap();
                    runtime.spawn(async move {
                        // Create SSH config
                        let config = client::Config::default();
                        let sh = ClientHandler {};

                        // Connect to SSH server
                        let mut session = timeout(
                            Duration::from_secs(timeout_secs as u64),
                            client::connect(Arc::new(config), (host.as_str(), port as u16), sh),
                        )
                        .await
                        .map_err(|_| "Connection timeout".to_string())?
                        .map_err(|e| format!("Failed to connect: {}", e))?;

                        // Authenticate
                        if let Some(key_path) = key_path {
                            let key_path_expanded = shellexpand::tilde(&key_path).to_string();
                            let key = load_secret_key(key_path_expanded, None)
                                .map_err(|e| format!("Failed to load SSH key: {}", e))?;
                            session
                                .authenticate_publickey(user.clone(), Arc::new(key))
                                .await
                                .map_err(|e| format!("Authentication failed: {}", e))?;
                        } else if let Some(password) = password {
                            session
                                .authenticate_password(user.clone(), password)
                                .await
                                .map_err(|e| format!("Password authentication failed: {}", e))?;
                        } else {
                            return Err("Either KeyPath or Password must be provided".to_string());
                        }

                        // Open channel and request PTY
                        let channel = session
                            .channel_open_session()
                            .await
                            .map_err(|e| format!("Failed to open channel: {}", e))?;

                        channel
                            .request_pty(false, "xterm", 80, 24, 0, 0, &[])
                            .await
                            .map_err(|e| format!("Failed to request PTY: {}", e))?;

                        // Request shell
                        channel
                            .request_shell(false)
                            .await
                            .map_err(|e| format!("Failed to request shell: {}", e))?;

                        // Drain initial prompt (wait for shell to be ready)
                        tokio::time::sleep(Duration::from_millis(500)).await;
                        let mut output_buffer = String::new();
                        for _ in 0..10 {
                            match timeout(Duration::from_millis(100), channel.wait()).await {
                                Ok(Some(msg)) => {
                                    if let ChannelMsg::Data { ref data } = msg {
                                        let chunk = String::from_utf8_lossy(data);
                                        output_buffer.push_str(&chunk);
                                        if is_prompt(&output_buffer) {
                                            break;
                                        }
                                    }
                                }
                                _ => break,
                            }
                        }

                        shlog_debug!("SSH connection established, shell ready");

                        // Create SSHShell object
                        let ssh_shell = SSHShell {
                            session,
                            channel,
                            pending_interactive: Arc::new(Mutex::new(None)),
                        };

                        let shell_var = Var::new_ref_counted(ssh_shell, &*SSH_SHELL_TYPE);
                        Ok(shell_var.into())
                    })
                };

                task.await
                    .map_err(|e| shards::core::FastError::Dynamic(e.to_string()))?
                    .map_err(|s| shards::core::FastError::Dynamic(s))
            },
            || {
                shlog_debug!("SSH connection cancelled");
            },
        );

        match result {
            Ok(output) => {
                self.output = output;
                Ok(Some(self.output.0))
            }
            Err(e) => {
                shlog_error!("SSH.Connect failed: {}", e);
                Err("SSH.Connect failed")
            }
        }
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

    #[shard_param("Session", "SSH shell session object", [*SSH_SHELL_TYPE])]
    session: ParamVar,

    #[shard_param("Timeout", "Command timeout in seconds (default: 30)", [common_type::int])]
    timeout_secs: ParamVar,

    #[shard_param("CleanOutput", "Clean ANSI codes and prompts from output (default: true)", [common_type::bool])]
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
        let cmd: &str = input.try_into()?;
        let session_var = *self.session.get();
        let timeout_secs: i64 = self.timeout_secs.get().as_ref().try_into()?;
        let should_clean: bool = self.clean_output.get().as_ref().try_into()?;

        let cmd = cmd.to_string();

        let result = run_future(
            context,
            async move {
                let ssh_shell = unsafe {
                    Var::from_ref_counted_object::<SSHShell>(&session_var, &*SSH_SHELL_TYPE)
                };
                let ssh_shell = unsafe { &mut *(ssh_shell? as *mut SSHShell) };

                let runtime = TOKIO_RUNTIME.clone();
                let task: tokio::task::JoinHandle<Result<ClonedVar, String>> = {
                    let runtime = runtime.lock().unwrap();
                    let channel = ssh_shell.channel;
                    let pending_interactive = ssh_shell.pending_interactive.clone();
                    let cmd_clone = cmd.clone();

                    runtime.spawn(async move {
                        // Check if there's a pending interactive command
                        {
                            let mut pending = pending_interactive.lock().unwrap();
                            if pending.is_some() {
                                shlog_info!(
                                    "New command received while interactive command was pending, cancelling previous"
                                );
                                // Send Ctrl+C to cancel
                                channel
                                    .data(&[3])
                                    .await
                                    .map_err(|e| format!("Failed to send interrupt: {}", e))?;
                                tokio::time::sleep(Duration::from_millis(100)).await;
                                // Clear pending state
                                *pending = None;
                            }
                        }

                        // Send command
                        let cmd_with_newline = format!("{}\n", cmd_clone);
                        channel
                            .data(cmd_with_newline.as_bytes())
                            .await
                            .map_err(|e| format!("Failed to send command: {}", e))?;

                        // Read output with timeout-based prompt detection
                        let mut output_buffer = String::new();
                        let mut timeout_count = 0;
                        let max_timeout_count = 30; // 3 seconds total (30 * 100ms)
                        let mut prompt_detected = false;

                        for _ in 0..max_timeout_count {
                            match timeout(Duration::from_millis(100), channel.wait()).await {
                                Ok(Some(msg)) => {
                                    if let ChannelMsg::Data { ref data } = msg {
                                        let chunk = String::from_utf8_lossy(data);
                                        output_buffer.push_str(&chunk);

                                        // Check for prompt
                                        if is_prompt(&output_buffer) {
                                            prompt_detected = true;
                                            shlog_debug!("Prompt detected, command completed");
                                            break;
                                        }
                                        timeout_count = 0; // Reset timeout count on new data
                                    }
                                }
                                _ => {
                                    timeout_count += 1;
                                    if timeout_count >= 15 && !output_buffer.is_empty() {
                                        // No output for 1.5 seconds, likely interactive
                                        shlog_debug!("Command appears to be interactive (timeout without prompt)");
                                        break;
                                    }
                                }
                            }
                        }

                        // Determine status and prepare output
                        let mut result_table = AutoTableVar::new();

                        if prompt_detected {
                            // Command completed normally
                            let final_output = if should_clean {
                                clean_output(&output_buffer, &cmd_clone)
                            } else {
                                output_buffer
                            };

                            result_table.0.insert_fast_static("status", &Var::ephemeral_string("completed"));
                            result_table.0.insert_fast_static("output", &Var::ephemeral_string(&final_output));
                        } else {
                            // Command is interactive (waiting for input)
                            {
                                let mut pending = pending_interactive.lock().unwrap();
                                *pending = Some(InteractiveState {
                                    original_cmd: cmd_clone.clone(),
                                });
                            }

                            let partial_output = if should_clean {
                                clean_output(&output_buffer, &cmd_clone)
                            } else {
                                output_buffer
                            };

                            result_table.0.insert_fast_static("status", &Var::ephemeral_string("requires_interaction"));
                            result_table.0.insert_fast_static("output", &Var::ephemeral_string(&partial_output));
                            result_table.0.insert_fast_static(
                                "message",
                                &Var::ephemeral_string("Command is waiting for input. Use SSH.SendInput to interact.")
                            );
                        }

                        Ok(result_table.to_cloned())
                    })
                };

                task.await
                    .map_err(|e| shards::core::FastError::Dynamic(e.to_string()))?
                    .map_err(|s| shards::core::FastError::Dynamic(s))
            },
            || {
                shlog_debug!("SSH.Execute cancelled");
            },
        );

        match result {
            Ok(output) => {
                self.output = output;
                Ok(Some(self.output.0))
            }
            Err(e) => {
                shlog_error!("SSH.Execute failed: {}", e);
                Err("SSH.Execute failed")
            }
        }
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

    #[shard_param("Session", "SSH shell session object", [*SSH_SHELL_TYPE])]
    session: ParamVar,

    #[shard_param("Timeout", "Timeout in seconds (default: 10)", [common_type::int])]
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
        let input_str: &str = input.try_into()?;
        let session_var = *self.session.get();
        let timeout_secs: i64 = self.timeout_secs.get().as_ref().try_into()?;

        let input_str = input_str.to_string();

        let result = run_future(
            context,
            async move {
                let ssh_shell = unsafe {
                    Var::from_ref_counted_object::<SSHShell>(&session_var, &*SSH_SHELL_TYPE)
                };
                let ssh_shell = unsafe { &mut *(ssh_shell? as *mut SSHShell) };

                let runtime = TOKIO_RUNTIME.clone();
                let task: tokio::task::JoinHandle<Result<ClonedVar, String>> = {
                    let runtime = runtime.lock().unwrap();
                    let channel = ssh_shell.channel;
                    let pending_interactive = ssh_shell.pending_interactive.clone();

                    runtime.spawn(async move {
                        // Check if there's a pending interactive command
                        {
                            let pending = pending_interactive.lock().unwrap();
                            if pending.is_none() {
                                return Err("No interactive command is pending".to_string());
                            }
                        }

                        // Send input (if not empty - empty means just check output)
                        if !input_str.is_empty() {
                            let input_with_newline = format!("{}\n", input_str);
                            channel
                                .data(input_with_newline.as_bytes())
                                .await
                                .map_err(|e| format!("Failed to send input: {}", e))?;
                        }

                        // Wait for output
                        tokio::time::sleep(Duration::from_secs(2)).await;

                        // Read output
                        let mut output_buffer = String::new();
                        for _ in 0..10 {
                            match timeout(Duration::from_millis(100), channel.wait()).await {
                                Ok(Some(msg)) => {
                                    if let ChannelMsg::Data { ref data } = msg {
                                        let chunk = String::from_utf8_lossy(data);
                                        output_buffer.push_str(&chunk);
                                    }
                                }
                                _ => break,
                            }
                        }

                        // Check for prompt
                        let prompt_detected = is_prompt(&output_buffer);

                        // Clean output
                        let stripped_bytes = strip_ansi_escapes::strip(&output_buffer);
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
                                let mut pending = pending_interactive.lock().unwrap();
                                *pending = None;
                            }
                            shlog_info!("Interactive session completed");
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

                        Ok(result_table.to_cloned())
                    })
                };

                task.await
                    .map_err(|e| shards::core::FastError::Dynamic(e.to_string()))?
                    .map_err(|s| shards::core::FastError::Dynamic(s))
            },
            || {
                shlog_debug!("SSH.SendInput cancelled");
            },
        );

        match result {
            Ok(output) => {
                self.output = output;
                Ok(Some(self.output.0))
            }
            Err(e) => {
                shlog_error!("SSH.SendInput failed: {}", e);
                Err("SSH.SendInput failed")
            }
        }
    }
}

// ============================================================================
// SSH.Disconnect Shard
// ============================================================================

#[derive(shards::shard)]
#[shard_info(
    "SSH.Disconnect",
    "Disconnect from SSH server and close the shell session"
)]
pub struct DisconnectShard {
    #[shard_required]
    required: ExposedTypes,
}

impl Default for DisconnectShard {
    fn default() -> Self {
        Self {
            required: ExposedTypes::new(),
        }
    }
}

#[shards::shard_impl]
impl Shard for DisconnectShard {
    fn input_types(&mut self) -> &Types {
        &SSH_SHELL_TYPE_VEC
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
        Ok(())
    }

    fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
        self.compose_helper(data)?;
        Ok(common_type::none)
    }

    fn activate(&mut self, context: &Context, input: &Var) -> Result<Option<Var>, &str> {
        let result = run_future(
            context,
            async move {
                let ssh_shell =
                    unsafe { Var::from_ref_counted_object::<SSHShell>(&input, &*SSH_SHELL_TYPE) };
                let ssh_shell = unsafe { &mut *(ssh_shell? as *mut SSHShell) };

                let runtime = TOKIO_RUNTIME.clone();
                let task: tokio::task::JoinHandle<Result<(), String>> = {
                    let runtime = runtime.lock().unwrap();
                    let channel = ssh_shell.channel;
                    let mut session = ssh_shell.session.clone();

                    runtime.spawn(async move {
                        // Close channel
                        channel
                            .eof()
                            .await
                            .map_err(|e| format!("Failed to send EOF: {}", e))?;

                        // Disconnect session
                        session
                            .disconnect(Disconnect::ByApplication, "", "English")
                            .await
                            .map_err(|e| format!("Failed to disconnect: {}", e))?;

                        shlog_debug!("SSH session disconnected");
                        Ok(())
                    })
                };

                task.await
                    .map_err(|e| shards::core::FastError::Dynamic(e.to_string()))?
                    .map_err(|s| shards::core::FastError::Dynamic(s))
            },
            || {
                shlog_debug!("SSH.Disconnect cancelled");
            },
        );

        match result {
            Ok(_) => Ok(None),
            Err(e) => {
                shlog_error!("SSH.Disconnect failed: {}", e);
                Err("SSH.Disconnect failed")
            }
        }
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
    register_shard::<DisconnectShard>();

    shlog_info!("SSH module registered");
}
