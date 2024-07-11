use std::io::{BufRead, BufReader, BufWriter, Write};

use interprocess::local_socket::LocalSocketListener;
use shards::core::{register_enum, register_shard, run_blocking, BlockingShard};
use shards::shard::Shard;
use shards::types::{common_type, ClonedVar, BYTES_TYPES, NONE_TYPES};
use shards::types::{Context, ExposedTypes, InstanceData, ParamVar, Type, Types, Var};

#[derive(shards::shard)]
#[shard_info("IPC.Take", "Reads a single message from a named socket")]
struct IPCTakeShard {
  #[shard_required]
  required: ExposedTypes,
  #[shard_param("Name", "Name of the socket", [common_type::string])]
  name: ClonedVar,
  read_buffer: Vec<u8>,
  listener: Option<LocalSocketListener>,
}

impl Default for IPCTakeShard {
  fn default() -> Self {
    Self {
      required: ExposedTypes::new(),
      name: ClonedVar::default(),
      read_buffer: Vec::new(),
      listener: None,
    }
  }
}

#[shards::shard_impl]
impl Shard for IPCTakeShard {
  fn input_types(&mut self) -> &Types {
    &NONE_TYPES
  }

  fn output_types(&mut self) -> &Types {
    &BYTES_TYPES
  }

  fn warmup(&mut self, ctx: &Context) -> Result<(), &str> {
    self.warmup_helper(ctx)?;

    Ok(())
  }

  fn cleanup(&mut self) -> Result<(), &str> {
    self.cleanup_helper()?;
    self.listener = None;

    Ok(())
  }

  fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
    self.compose_helper(data)?;
    if self.name.0.is_none() {
      return Err("Name is required");
    }
    Ok(self.output_types()[0])
  }

  fn activate(&mut self, context: &Context, input: &Var) -> Result<Var, &str> {
    Ok(run_blocking(self, context, input))
  }
}

impl BlockingShard for IPCTakeShard {
  fn activate_blocking(&mut self, context: &Context, input: &Var) -> Result<Var, &str> {

    if let None = self.listener {
      let socket_name: &str = (&self.name.0).try_into()?;
      self.listener = Some({
        let listener = interprocess::local_socket::LocalSocketListener::bind(socket_name)
          .map_err(|_| "Failed to connect")?;
        listener
          .set_nonblocking(false)
          .map_err(|_| "Failed to set non-blocking")?;
        listener
      });
    }

    let listener = self.listener.as_ref().unwrap();
    let mut socket = listener
      .accept()
      .map_err(|_| "Failed to accept client")
      .map(BufReader::new)?;

    let mut decoder = serial_line_ip::Decoder::new();

    loop {
      let stream_buf = socket
        .fill_buf()
        .map_err(|_| "Failed to read from stream")?;

      let (input_bytes_processed, output_slice, is_end_of_packet) = decoder
        .decode(&stream_buf, &mut self.read_buffer)
        .map_err(|_| "Failed to read frame")?;
      socket.consume(input_bytes_processed);

      if is_end_of_packet {
        let out_var: Var = self.read_buffer.as_slice().into();
        return Ok(out_var);
      }
    }
  }
  fn cancel_activation(&mut self, _context: &Context) {}
}

#[derive(shards::shard)]
#[shard_info("IPC.Post", "Reads a single message from a named socket")]
struct IPCPostShard {
  #[shard_required]
  required: ExposedTypes,
  #[shard_param("Name", "Name of the socket", [common_type::string])]
  name: ClonedVar,
  write_buffer: Vec<u8>,
}

impl Default for IPCPostShard {
  fn default() -> Self {
    Self {
      required: ExposedTypes::new(),
      name: ClonedVar::default(),
      write_buffer: Vec::new(),
    }
  }
}

#[shards::shard_impl]
impl Shard for IPCPostShard {
  fn input_types(&mut self) -> &Types {
    &BYTES_TYPES
  }

  fn output_types(&mut self) -> &Types {
    &NONE_TYPES
  }

  fn warmup(&mut self, ctx: &Context) -> Result<(), &str> {
    self.warmup_helper(ctx)?;

    Ok(())
  }

  fn cleanup(&mut self) -> Result<(), &str> {
    self.cleanup_helper()?;

    Ok(())
  }

  fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
    self.compose_helper(data)?;
    if self.name.0.is_none() {
      return Err("Name is required");
    }
    Ok(self.output_types()[0])
  }

  fn activate(&mut self, context: &Context, input: &Var) -> Result<Var, &str> {
    Ok(run_blocking(self, context, input))
  }
}

impl BlockingShard for IPCPostShard {
  fn activate_blocking(&mut self, _context: &Context, input: &Var) -> Result<Var, &str> {
    let socket_name: &str = (&self.name.0).try_into()?;
    let mut socket = interprocess::local_socket::LocalSocketStream::connect(socket_name)
      .map_err(|_| "Failed to connect")
      .map(BufReader::new)?;

    self.write_buffer.resize(1024, 0);

    let mut encoder = serial_line_ip::Encoder::new();

    let input_data: &[u8] = input.try_into()?;

    let totals = encoder
      .encode(input_data, &mut self.write_buffer)
      .map_err(|_| "Failed to encode frame")?;
    let mut offset = totals.written;

    let totals = encoder
      .finish(&mut self.write_buffer[offset..])
      .map_err(|_| "Failed to encode frame")?;
    offset += totals.written;

    socket
      .get_mut()
      .write_all(&self.write_buffer[..(offset + 1)])
      .map_err(|_| "Failed to write to stream")?;

    Ok(Var::default())
  }
  fn cancel_activation(&mut self, _context: &Context) {}
}

#[no_mangle]
pub extern "C" fn shardsRegister_ipc_rust(core: *mut shards::shardsc::SHCore) {
  unsafe {
    shards::core::Core = core;
  }

  register_shard::<IPCTakeShard>();
  register_shard::<IPCPostShard>();
}
