use anyhow::anyhow;
use futures::executor::block_on;
use futures_util::{SinkExt, StreamExt};
use log::{debug, error, info, warn};
use slotmap::{HopSlotMap, Key};
use std::net::SocketAddr;
use std::thread;
use tokio::io::AsyncWriteExt;
use tokio::net::{TcpListener, TcpStream};
use tokio::runtime;
use tokio_tungstenite::{accept_async, connect_async};

mod wsfuncs;

// SendErrors are ironically not themselves sync so this
// function just turns any send error into an anyhow string error
fn check_tx<T>(r: Result<(), std::sync::mpsc::SendError<T>>) -> anyhow::Result<()> {
  r.map_err(|_| anyhow!("Channel TX Error"))
}

fn send_error<T>(tx: std::sync::mpsc::Sender<PollnetMessage>, msg: T)
where
  T: ToString,
{
  if tx.send(PollnetMessage::Error(msg.to_string())).is_err() {
    warn!("TX error sending error.")
  }
}

fn send_disconnect(tx: std::sync::mpsc::Sender<PollnetMessage>) {
  if tx.send(PollnetMessage::Disconnect).is_err() {
    warn!("TX error sending disconnect.")
  }
}

slotmap::new_key_type! {
  pub struct SocketHandle;
}

impl From<u64> for SocketHandle {
  fn from(item: u64) -> Self {
    Self::from(slotmap::KeyData::from_ffi(item))
  }
}
impl From<SocketHandle> for u64 {
  fn from(item: SocketHandle) -> Self {
    item.data().as_ffi()
  }
}

#[derive(Debug)]
pub enum RecvError {
  Empty,
  Disconnected,
}

#[derive(Copy, Clone)]
pub enum SocketStatus {
  InvalidHandle,
  Error,
  Closed,
  Opening,
  OpenNoData,
  OpenHasData,
  OpenNewClient,
}

impl From<SocketStatus> for u32 {
  fn from(item: SocketStatus) -> Self {
    item as u32
  }
}

struct HostChannels {
  pub tx: tokio::sync::mpsc::Sender<PollnetMessage>,
  pub rx: std::sync::mpsc::Receiver<PollnetMessage>,
}

struct ReactorChannels {
  pub tx: std::sync::mpsc::Sender<PollnetMessage>,
  pub rx: tokio::sync::mpsc::Receiver<PollnetMessage>,
}

fn create_channels() -> (HostChannels, ReactorChannels) {
  let (tx_to_sock, rx_to_sock) = tokio::sync::mpsc::channel(100);
  let (tx_from_sock, rx_from_sock) = std::sync::mpsc::channel();
  let hc = HostChannels {
    tx: tx_to_sock,
    rx: rx_from_sock,
  };
  let rc = ReactorChannels {
    tx: tx_from_sock,
    rx: rx_to_sock,
  };
  (hc, rc)
}

struct ClientConn {
  io: HostChannels,
  id: String,
}

enum PollnetMessage {
  Connect,
  Disconnect,
  Text(String),
  Binary(Vec<u8>),
  Error(String),
  NewClient(ClientConn),
}

pub struct PollnetSocket {
  pub status: SocketStatus,
  io: Option<HostChannels>,
  pub data: Option<Vec<u8>>,
  pub last_client_handle: SocketHandle,
}

pub struct PollnetContext {
  pub sockets: HopSlotMap<SocketHandle, Box<PollnetSocket>>,
  thread: Option<thread::JoinHandle<()>>,
  rt_handle: tokio::runtime::Handle,
  shutdown_tx: Option<tokio::sync::oneshot::Sender<i32>>,
}

impl PollnetContext {
  pub fn new() -> PollnetContext {
    let (handle_tx, handle_rx) = std::sync::mpsc::channel();
    let (shutdown_tx, shutdown_rx) = tokio::sync::oneshot::channel();
    let shutdown_tx = Some(shutdown_tx);

    let thread = Some(thread::spawn(move || {
      let rt = runtime::Builder::new_current_thread()
        .enable_all()
        .build()
        .expect("Unable to create the runtime");

      // Send handle back out so we can store it?
      handle_tx
        .send(rt.handle().clone())
        .expect("Unable to give runtime handle to another thread");

      // Continue running until notified to shutdown
      info!("tokio runtime starting");
      rt.block_on(async {
        shutdown_rx.await.unwrap();
        // uh let's just put in a 'safety' delay to shut everything down?
        tokio::time::sleep(std::time::Duration::from_millis(200)).await;
      });
      rt.shutdown_timeout(std::time::Duration::from_millis(200));
      info!("tokio runtime shutdown");
    }));

    PollnetContext {
      rt_handle: handle_rx.recv().unwrap(),
      thread,
      shutdown_tx,
      sockets: HopSlotMap::with_key(),
    }
  }

  pub fn close_all(&mut self) {
    info!("Closing all sockets!");
    for (_, sock) in self.sockets.iter_mut() {
      if let Some(chans) = &sock.io {
        block_on(chans.tx.send(PollnetMessage::Disconnect)).unwrap_or_default();
      }
      sock.io.take();
      sock.status = SocketStatus::Closed;
    }
    self.sockets.clear(); // everything should be closed and safely droppable
  }

  pub fn close(&mut self, handle: SocketHandle) {
    if let Some(sock) = self.sockets.get_mut(handle) {
      if let Some(chans) = &sock.io {
        if let Err(e) = block_on(chans.tx.send(PollnetMessage::Disconnect)) {
          warn!("Socket already closed from other end: {:}", e);
        }
      }
      debug!("Removing handle: {:?}", handle);
      // Note: since we don't wait here for any kind of "disconnect" reply,
      // a socket that has been closed should just return without sending a reply
      self.sockets.remove(handle);
    }
  }

  pub fn send(&mut self, handle: SocketHandle, msg: String) {
    if let Some(sock) = self.sockets.get_mut(handle) {
      if let Some(chans) = &sock.io {
        chans
          .tx
          .try_send(PollnetMessage::Text(msg))
          .unwrap_or_default();
      }
    }
  }

  pub fn send_binary(&mut self, handle: SocketHandle, msg: &[u8]) {
    if let Some(sock) = self.sockets.get_mut(handle) {
      if let Some(chans) = &sock.io {
        chans
          .tx
          .try_send(PollnetMessage::Binary(msg.to_vec()))
          .unwrap_or_default();
      };
    }
  }

  pub fn update(&mut self, handle: SocketHandle, blocking: bool) -> SocketStatus {
    let sock = match self.sockets.get_mut(handle) {
      Some(sock) => sock,
      None => return SocketStatus::InvalidHandle,
    };

    let rx = match &sock.io {
      Some(chans) => &chans.rx,
      None => return sock.status,
    };

    // This block is apparently impossible to move into a helper function
    // for borrow checker "reasons"
    let result = if blocking {
      rx.recv().map_err(|_err| RecvError::Disconnected)
    } else {
      rx.try_recv().map_err(|err| match err {
        std::sync::mpsc::TryRecvError::Empty => RecvError::Empty,
        std::sync::mpsc::TryRecvError::Disconnected => RecvError::Disconnected,
      })
    };

    match result {
      Ok(PollnetMessage::Connect) => {
        sock.status = SocketStatus::OpenNoData;
        sock.status
      }
      Ok(PollnetMessage::Disconnect) | Err(RecvError::Disconnected) => {
        debug!("Socket disconnected.");
        sock.io.take();
        sock.status = SocketStatus::Closed;
        sock.status
      }
      Ok(PollnetMessage::Text(msg)) => {
        debug!("Socket text message {:} bytes", msg.len());
        sock.data = Some(msg.into_bytes());
        sock.status = SocketStatus::OpenHasData;
        sock.status
      }
      Ok(PollnetMessage::Binary(msg)) => {
        debug!("Socket binary message {:} bytes", msg.len());
        sock.data = Some(msg);
        sock.status = SocketStatus::OpenHasData;
        sock.status
      }
      Ok(PollnetMessage::Error(err)) => {
        error!("Socket error: {:}", err);
        sock.data = Some(err.into_bytes());
        sock.io.take();
        sock.status = SocketStatus::Error;
        sock.status
      }
      Ok(PollnetMessage::NewClient(conn)) => {
        sock.data = Some(conn.id.into_bytes());
        sock.status = SocketStatus::OpenNewClient;
        let client_socket = Box::new(PollnetSocket {
          io: Some(conn.io),
          status: SocketStatus::OpenNoData, // assume client sockets start open?
          data: None,
          last_client_handle: SocketHandle::null(),
        });
        let newhandle = self.sockets.insert(client_socket);
        // Note this horrible hack so it won't complain about multiple mutable borrows
        // of self.sockets at the same time
        let sock2 = match self.sockets.get_mut(handle) {
          Some(sock) => sock,
          None => return SocketStatus::InvalidHandle,
        };
        sock2.last_client_handle = newhandle;
        SocketStatus::OpenNewClient
      }
      Err(RecvError::Empty) => {
        // clear previous data if any
        sock.data.take();
        match sock.status {
          SocketStatus::Opening => SocketStatus::Opening,
          _ => SocketStatus::OpenNoData,
        }
      }
    }
  }

  pub fn shutdown(&mut self) {
    info!("Starting shutdown");
    self.close_all();
    info!("All sockets should be closed?");
    if let Some(tx) = self.shutdown_tx.take() {
      tx.send(0).unwrap_or_default();
    }
    if let Some(handle) = self.thread.take() {
      handle.join().unwrap_or_default();
    }
    info!("Thread should be joined?");
  }
}

impl Default for PollnetContext {
  fn default() -> Self {
    Self::new()
  }
}
