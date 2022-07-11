use crate::{
  core::{activate_blocking, registerShard, suspend, BlockingShard},
  types::{
    ClonedVar, Context, ExposedInfo, ExposedTypes, ParamVar, Parameters, Type, Types, Var,
    WireState, ANYS_TYPES, FRAG_CC, NONE_TYPES, STRING_TYPES,
  },
  Shard,
};
use instant::Duration;
use std::rc::Rc;
use std::thread::spawn;
use std::{io::ErrorKind, net::TcpStream};
use tungstenite::{
  accept, handshake::client::Response, stream::MaybeTlsStream, Message, WebSocket,
};

// TODO EXPOSE AND REQUIRE VARIABLES

static WS_CLIENT_TYPE: Type = Type::object(FRAG_CC, 2004042604); // 'wsCl'
static WS_CLIENT_SLICE: &'static [Type] = &[WS_CLIENT_TYPE];
static WS_CLIENT_VAR: Type = Type::context_variable(WS_CLIENT_SLICE);

lazy_static! {
  static ref WS_CLIENT_VEC: Types = vec![WS_CLIENT_TYPE];
  static ref WS_CLIENT_VAR_VEC: Types = vec![WS_CLIENT_VAR];
  static ref WS_CLIENT_USER_PARAMS: Parameters = vec![(
    cstr!("Client"),
    cstr!("The WebSocket client instance."),
    &WS_CLIENT_VAR_VEC[..],
  )
    .into(),];
}

#[derive(Default)]
struct Client {
  ws: Rc<Option<WebSocket<MaybeTlsStream<TcpStream>>>>,
}

impl Shard for Client {
  fn registerName() -> &'static str
  where
    Self: Sized,
  {
    cstr!("WS.Client")
  }

  fn hash() -> u32
  where
    Self: Sized,
  {
    compile_time_crc32::crc32!("WS.Client-rust-0x20200101")
  }

  fn name(&mut self) -> &str {
    "WS.Client"
  }

  fn inputTypes(&mut self) -> &Types {
    &STRING_TYPES
  }

  fn outputTypes(&mut self) -> &Types {
    &WS_CLIENT_VEC
  }

  fn activate(&mut self, context: &Context, input: &Var) -> Result<Var, &str> {
    Ok(activate_blocking(self, context, input))
  }
}

impl BlockingShard for Client {
  fn run_blocking(&mut self, _context: &Context, input: &Var) -> Result<Var, &str> {
    let addr: &str = input.try_into()?;
    let mut ws = tungstenite::client::connect(addr).map_err(|e| {
      shlog!("{}", e);
      "Failed to connect to server."
    })?;

    // setup timeouts, we don't want to wait forever inside our thread pool for a response
    // and so every 100ms we yield to allow coroutine scheduler suspension
    let stream = ws.0.get_mut();
    match *stream {
      MaybeTlsStream::Plain(ref mut s) => s.set_read_timeout(Some(Duration::new(0, 100_000_000))),
      MaybeTlsStream::Rustls(ref mut s) => {
        s.sock.set_read_timeout(Some(Duration::new(0, 100_000_000)))
      }
      _ => unimplemented!(),
    }
    .map_err(|e| {
      shlog!("{}", e);
      "Failed to set read timeout."
    })?;

    // we need to keep this alive here, otherwise it will be dropped at the end of this function
    self.ws = Rc::new(Some(ws.0));
    let var_ws = Var::new_object(&self.ws, &WS_CLIENT_TYPE);
    Ok(var_ws)
  }
}

#[derive(Default)]
struct ClientUser {
  ws: Rc<Option<WebSocket<MaybeTlsStream<TcpStream>>>>,
  instance: ParamVar,
  requiring: Vec<ExposedInfo>,
}

impl ClientUser {
  fn set_param(&mut self, index: i32, value: &Var) {
    match index {
      0 => self.instance.set_param(value),
      _ => panic!("Invalid index {}", index),
    }
  }

  fn get_param(&self, index: i32) -> Var {
    match index {
      0 => self.instance.get_param(),
      _ => panic!("Invalid index {}", index),
    }
  }

  fn required_variables(&mut self) -> Option<&ExposedTypes> {
    self.requiring.clear();

    // Add GUI.Context to the list of required variables
    let exp_info = ExposedInfo {
      exposedType: WS_CLIENT_TYPE,
      name: self.instance.get_name(),
      help: cstr!("The exposed UI context.").into(),
      ..ExposedInfo::default()
    };
    self.requiring.push(exp_info);

    Some(&self.requiring)
  }

  fn warmup(&mut self, context: &Context) -> Result<(), &str> {
    self.instance.warmup(context);
    Ok(())
  }

  fn cleanup(&mut self) -> Result<(), &str> {
    self.instance.cleanup();
    Ok(())
  }

  fn activate(&mut self) -> Result<&mut WebSocket<MaybeTlsStream<TcpStream>>, &str> {
    if self.ws.is_none() {
      let ws = self.instance.get();
      self.ws = Var::from_object_as_clone(ws, &WS_CLIENT_TYPE)?;
    }

    let ws = Var::from_object_mut_ref(self.instance.get(), &WS_CLIENT_TYPE)?;

    Ok(ws)
  }
}

#[derive(Default)]
struct ReadString {
  client: ClientUser,
  text: ClonedVar,
}

impl Shard for ReadString {
  fn registerName() -> &'static str
  where
    Self: Sized,
  {
    cstr!("WS.ReadString")
  }

  fn hash() -> u32
  where
    Self: Sized,
  {
    compile_time_crc32::crc32!("WS.ReadString-rust-0x20200101")
  }

  fn name(&mut self) -> &str {
    "WS.ReadString"
  }

  fn inputTypes(&mut self) -> &Types {
    &NONE_TYPES
  }

  fn outputTypes(&mut self) -> &Types {
    &STRING_TYPES
  }

  fn parameters(&mut self) -> Option<&Parameters> {
    Some(&WS_CLIENT_USER_PARAMS)
  }

  fn requiredVariables(&mut self) -> Option<&ExposedTypes> {
    self.client.required_variables()
  }

  fn setParam(&mut self, index: i32, value: &Var) -> Result<(), &str> {
    self.client.set_param(index, value);
    Ok(())
  }

  fn getParam(&mut self, index: i32) -> Var {
    self.client.get_param(index)
  }

  fn warmup(&mut self, context: &Context) -> Result<(), &str> {
    self.client.warmup(context)
  }

  fn cleanup(&mut self) -> Result<(), &str> {
    self.client.cleanup()
  }

  fn activate(&mut self, context: &Context, input: &Var) -> Result<Var, &str> {
    // stay away from sys calls from main coroutine
    loop {
      let result = activate_blocking(self, context, input);
      // a bit of an hack, use bool as none would return if we had an error!
      if result.is_bool() {
        // none means we should yield
        // shlog_debug!("ReadString: yield");
        let next_state = suspend(context, 0.0);
        if next_state != WireState::Continue {
          return Err("Stopped");
        }
        continue;
      } else {
        return Ok(result);
      }
    }
  }
}

impl BlockingShard for ReadString {
  fn run_blocking(&mut self, _context: &Context, _input: &Var) -> Result<Var, &str> {
    let ws = self.client.activate()?;
    loop {
      let pending_data = {
        let stream = ws.get_mut();
        let mut buf = [0u8; 1];
        match *stream {
          MaybeTlsStream::Plain(ref mut s) => s.peek(&mut buf),
          MaybeTlsStream::Rustls(ref mut s) => s.sock.peek(&mut buf),
          _ => unimplemented!(),
        }
      };

      match pending_data {
        Ok(_) => {
          let msg = ws.read_message().map_err(|e| {
            shlog!("{}", e);
            "Failed to read message."
          })?;

          match msg {
            Message::Text(text) => {
              self.text = text.into();
              return Ok(self.text.0);
            }
            Message::Binary(_) => {
              return Err("Received binary message, expected text.");
            }
            Message::Close(packet) => {
              shlog!("Received close message: {:?}", packet);
              return Err("Received close message, expected text.");
            }
            Message::Ping(_) => {
              // shlog_debug!("Received ping message, sending pong.");
              ws.write_message(Message::Pong(vec![])).map_err(|e| {
                shlog!("{}", e);
                "Failed to write message."
              })?;
            }
            Message::Pong(_) => {
              return Err("Received pong message, expected text.");
            }
            _ => return Err("Invalid message type."),
          }
        }
        Err(e) => {
          if e.kind() == ErrorKind::TimedOut {
            return Ok(false.into());
          } else {
            shlog!("ReadMessage failed with error: {}", e);
            return Err("Failed to read message.");
          }
        }
      }
    }
  }
}

#[derive(Default)]
struct WriteString {
  client: ClientUser,
}

impl Shard for WriteString {
  fn registerName() -> &'static str
  where
    Self: Sized,
  {
    cstr!("WS.WriteString")
  }

  fn hash() -> u32
  where
    Self: Sized,
  {
    compile_time_crc32::crc32!("WS.WriteString-rust-0x20200101")
  }

  fn name(&mut self) -> &str {
    "WS.WriteString"
  }

  fn inputTypes(&mut self) -> &Types {
    &STRING_TYPES
  }

  fn outputTypes(&mut self) -> &Types {
    &STRING_TYPES
  }

  fn parameters(&mut self) -> Option<&Parameters> {
    Some(&WS_CLIENT_USER_PARAMS)
  }

  fn requiredVariables(&mut self) -> Option<&ExposedTypes> {
    self.client.required_variables()
  }

  fn setParam(&mut self, index: i32, value: &Var) -> Result<(), &str> {
    self.client.set_param(index, value);
    Ok(())
  }

  fn getParam(&mut self, index: i32) -> Var {
    self.client.get_param(index)
  }

  fn warmup(&mut self, context: &Context) -> Result<(), &str> {
    self.client.warmup(context)
  }

  fn cleanup(&mut self) -> Result<(), &str> {
    self.client.cleanup()
  }

  fn activate(&mut self, context: &Context, input: &Var) -> Result<Var, &str> {
    Ok(activate_blocking(self, context, input))
  }
}

impl BlockingShard for WriteString {
  fn run_blocking(&mut self, _context: &Context, input: &Var) -> Result<Var, &str> {
    let ws = self.client.activate()?;

    let msg: &str = input.try_into()?;

    ws.write_message(Message::Text(msg.to_string()))
      .map_err(|e| {
        shlog!("{}", e);
        "Failed to send message."
      })?;

    Ok(*input)
  }
}

pub fn registerShards() {
  registerShard::<Client>();
  registerShard::<ReadString>();
  registerShard::<WriteString>();
}
