use std::io::{self, BufRead, BufReader, BufWriter, Read, Write};

use interprocess::local_socket::traits::{Listener, ListenerExt, Stream};
use interprocess::local_socket::ToNsName;
use interprocess::*;
use shards::core::{register_enum, register_shard, run_blocking, BlockingShard};
use shards::shard::Shard;
use shards::types::{common_type, AutoSeqVar, ClonedVar, SeqVar, BYTES_TYPES, NONE_TYPES};
use shards::types::{Context, ExposedTypes, InstanceData, ParamVar, Type, Types, Var};
use shards::{shlog_debug, shlog_warn};

lazy_static::lazy_static! {
  static ref BYTES_SEQ_TYPE: Type = Type::seq(&BYTES_TYPES[..]);
  static ref BYTES_SEQ_TYPES: Types = vec![*BYTES_SEQ_TYPE];
}

#[derive(shards::shard)]
#[shard_info("IPC.Take", "Reads a single message from a named socket")]
struct IPCTakeShard {
  #[shard_required]
  required: ExposedTypes,
  #[shard_param("Name", "Name of the socket", [common_type::string])]
  name: ClonedVar,
  output: AutoSeqVar,
  read_buffer: Vec<u8>,
  listener: Option<local_socket::Listener>,
}

impl Default for IPCTakeShard {
  fn default() -> Self {
    Self {
      required: ExposedTypes::new(),
      name: ClonedVar::default(),
      output: AutoSeqVar::new(),
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
    &BYTES_SEQ_TYPES
  }

  fn warmup(&mut self, ctx: &Context) -> Result<(), &str> {
    self.warmup_helper(ctx)?;

    Ok(())
  }

  fn cleanup(&mut self, ctx: Option<&Context>) -> Result<(), &str> {
    self.cleanup_helper(ctx)?;
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
    self.output.0.clear();

    let mut close_after = false;
    if let Some(listener) = &self.listener {
      match listener.accept() {
        // Check for would block error
        Err(ref e) if e.kind() == io::ErrorKind::WouldBlock => {}
        Err(e) => {
          shlog_warn!("Incoming connection failed: {}", e);
          close_after = true;
        }
        Ok(mut stream) => {
          let mut line_decoder = serial_line_ip::Decoder::new();

          let mut read_buf: [u8; 512] = [0; 512];
          let mut decode_buf: [u8; 512] = [0; 512];
          loop {
            let in_bytes = stream
              .read(&mut read_buf)
              .map_err(|_| "Failed to read from stream")?;

            let (_input_bytes_processed, output_slice, is_end_of_packet) = line_decoder
              .decode(&read_buf[..in_bytes], &mut decode_buf)
              .map_err(|_| "Failed to read frame")?;

            self.read_buffer.extend_from_slice(output_slice);

            if is_end_of_packet {
              let out_var: Var = self.read_buffer.as_slice().into();
              self.output.0.push(&out_var);
              break;
            }
          }

          // Send ok byte
          let confirm_buf: [u8; 1] = [0];
          stream
            .write(&confirm_buf)
            .map_err(|_| "Failed to write to stream")?;
          stream.flush().map_err(|_| "Failed to flush stream")?;

          close_after = true;
        }
      }
    } else {
      let socket_name: &str = (&self.name.0).try_into()?;
      let name = socket_name
        .to_ns_name::<local_socket::GenericNamespaced>()
        .map_err(|e| "Failed to convert name")?;
      let opts = local_socket::ListenerOptions::new()
        .name(name)
        .nonblocking(local_socket::ListenerNonblockingMode::Accept);
      self.listener = Some(
        opts
          .create_sync()
          .map_err(|_e| "Failed to bind to local socket")?,
      );
    }

    if close_after {
      self.listener = None;
    }

    return Ok(self.output.0 .0);
  }
}

// impl BlockingShard for IPCTakeShard {
//   fn activate_blocking(&mut self, context: &Context, input: &Var) -> Result<Var, &str> {

//     if let None = self.listener {
//       let socket_name: &str = (&self.name.0).try_into()?;
//       self.listener = Some({
//         let listener = interprocess::local_socket::Listenerbind(socket_name)
//           .map_err(|_| "Failed to connect")?;
//         listener
//           .set_nonblocking(false)
//           .map_err(|_| "Failed to set non-blocking")?;
//         listener
//       });
//     }

//     let listener = self.listener.as_ref().unwrap();
//     let mut socket = listener
//       .accept()
//       .map_err(|_| "Failed to accept client")
//       .map(BufReader::new)?;

//     let mut decoder = serial_line_ip::Decoder::new();

//     loop {
//       let stream_buf = socket
//         .fill_buf()
//         .map_err(|_| "Failed to read from stream")?;

//       let (input_bytes_processed, output_slice, is_end_of_packet) = decoder
//         .decode(&stream_buf, &mut self.read_buffer)
//         .map_err(|_| "Failed to read frame")?;
//       socket.consume(input_bytes_processed);

//       if is_end_of_packet {
//         let out_var: Var = self.read_buffer.as_slice().into();
//         return Ok(out_var);
//       }
//     }
//   }
//   fn cancel_activation(&mut self, _context: &Context) {}
// }

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

  fn cleanup(&mut self, ctx: Option<&Context>) -> Result<(), &str> {
    self.cleanup_helper(ctx)?;

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
    let name = socket_name
      .to_ns_name::<local_socket::GenericNamespaced>()
      .map_err(|e| "Failed to convert name")?;

    let mut conn = interprocess::local_socket::Stream::connect(name)
      .map_err(|_| "Failed to connect")
      .map(BufReader::new)?;

    let input_data: &[u8] = input.try_into()?;

    let mut line_encoder = serial_line_ip::Encoder::new();
    let mut encoded_buf: Vec<u8> = Vec::new();
    let mut encoder_buf: [u8; 512] = [0; 512];
    let mut read_ptr = 0;
    loop {
      let totals = line_encoder
        .encode(&input_data[read_ptr..], &mut encoder_buf)
        .map_err(|_| "Failed to encode")?;
      read_ptr += totals.read;
      encoded_buf.extend_from_slice(&encoder_buf[..totals.written]);
      if read_ptr >= input_data.len() {
        break;
      }
    }

    let totals = line_encoder
      .finish(&mut encoder_buf)
      .map_err(|_| "Failed to finish encoding")?;
    encoded_buf.extend_from_slice(&encoder_buf[..totals.written]);

    conn
      .get_mut()
      .write_all(&encoded_buf)
      .map_err(|_| "Failed to write to stream")?;

    conn
      .get_mut()
      .flush()
      .map_err(|_| "Failed to flush stream")?;

    let mut confirm_buf: [u8; 1] = [0];
    conn
      .read(&mut confirm_buf)
      .map_err(|_| "Failed to read from stream")?;

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
