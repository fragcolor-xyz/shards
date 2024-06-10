use tokio::io::AsyncRead;
use tokio::io::AsyncWrite;
use tokio_tungstenite::WebSocketStream;
use tungstenite::protocol::Message;

use super::*;

async fn websocket_poll_loop_inner<S>(
  ws_stream: &mut WebSocketStream<S>,
  mut io: ReactorChannels,
) -> anyhow::Result<()>
where
  S: AsyncRead + AsyncWrite + Unpin,
{
  check_tx(io.tx.send(PollnetMessage::Connect))?;
  loop {
    tokio::select! {
        from_c_message = io.rx.recv() => {
            match from_c_message {
                Some(PollnetMessage::Text(msg)) => {
                    debug!("WS outgoing text: {:}", msg.len());
                    if let Err(e) = ws_stream.send(Message::Text(msg)).await {
                        debug!("WS send error.");
                        send_error(io.tx, e);
                        return Ok(());
                    };
                },
                Some(PollnetMessage::Binary(msg)) => {
                    debug!("WS outgoing binary: {:}", msg.len());
                    if let Err(e) = ws_stream.send(Message::Binary(msg)).await {
                        debug!("WS send error.");
                        send_error(io.tx, e);
                        return Ok(());
                    };
                },
                Some(PollnetMessage::Disconnect) => {
                    debug!("Client-side disconnect.");
                    return Ok(());
                },
                None => {
                    warn!("Channel closed w/o disconnect message.");
                    return Ok(());
                },
                _ => {
                    error!("Invalid message to WS!");
                }
            }
        },
        from_sock_message = ws_stream.next() => {
            match from_sock_message {
                Some(Ok(msg)) => {
                    if msg.is_text() || msg.is_binary() {
                        debug!("WS incoming msg: {:}", msg.len());
                        check_tx(io.tx.send(PollnetMessage::Binary(msg.into_data())))?;
                    } else if msg.is_close() {
                        debug!("Received close!");
                        send_disconnect(io.tx);
                        return Ok(());
                    }
                },
                Some(Err(msg)) => {
                    info!("WS error.");
                    send_error(io.tx, msg);
                    return Ok(());
                },
                None => {
                    info!("WS disconnect.");
                    send_disconnect(io.tx);
                    return Ok(());
                }
            }
        },
    };
  }
}

async fn websocket_poll_loop<S>(mut ws_stream: WebSocketStream<S>, io: ReactorChannels)
where
  S: AsyncRead + AsyncWrite + Unpin,
{
  if let Err(e) = websocket_poll_loop_inner(&mut ws_stream, io).await {
    error!("Unexpected WS loop termination: {:?}", e);
  }
  info!("Closing websocket!");
  // The stream might already be closed at this point but
  // tungstenite just logs a debug message so not a big deal
  ws_stream.close(None).await.unwrap_or_default();
}

async fn accept_ws_inner<S>(
  mut stream: S,
  addr: SocketAddr,
  outer_tx: std::sync::mpsc::Sender<PollnetMessage>,
) -> anyhow::Result<()>
where
  S: AsyncRead + AsyncWrite + Unpin,
{
  let (host_io, reactor_io) = create_channels();

  let sendres = outer_tx.send(PollnetMessage::NewClient(ClientConn {
    io: host_io,
    id: addr.to_string(),
  }));
  if sendres.is_err() {
    stream.shutdown().await.unwrap_or_default();
    return Err(anyhow!("Broken TX?"));
  }

  match accept_async(stream).await {
    Ok(ws_stream) => {
      websocket_poll_loop(ws_stream, reactor_io).await;
    }
    Err(err) => {
      error!("connection error: {}", err);
      send_error(reactor_io.tx, err);
    }
  }
  Ok(())
}

async fn accept_ws<S>(
  stream: S,
  addr: SocketAddr,
  outer_tx: std::sync::mpsc::Sender<PollnetMessage>,
) where
  S: AsyncRead + AsyncWrite + Unpin,
{
  if let Err(e) = accept_ws_inner(stream, addr, outer_tx).await {
    error!("Channel issue: {:?}", e);
  }
}

impl PollnetContext {
  pub fn open_ws(&mut self, url: &str) -> SocketHandle {
    let url = url.to_string();
    let (host_io, reactor_io) = create_channels();

    self.rt_handle.spawn(async move {
      info!("WS client spawned");
      let real_url = match url::Url::parse(&url) {
        Ok(v) => v,
        Err(url_err) => {
          error!("Invalid URL: {}", url);
          send_error(reactor_io.tx, url_err);
          return;
        }
      };

      info!("WS client attempting to connect to {}", url);
      match connect_async(real_url).await {
        Ok((ws_stream, _)) => {
          websocket_poll_loop(ws_stream, reactor_io).await;
        }
        Err(err) => {
          error!("WS client connection error: {}", err);
          send_error(reactor_io.tx, err);
        }
      }
    });

    let socket = Box::new(PollnetSocket {
      io: Some(host_io),
      status: SocketStatus::Opening,
      data: None,
      last_client_handle: SocketHandle::null(),
    });
    self.sockets.insert(socket)
  }

  pub fn listen_ws(&mut self, addr: &str) -> SocketHandle {
    let addr = addr.to_string();
    let (host_io, mut reactor_io) = create_channels();

    self.rt_handle.spawn(async move {
      info!("WS server spawned");
      let listener = match TcpListener::bind(&addr).await {
        Ok(listener) => listener,
        Err(tcp_err) => {
          send_error(reactor_io.tx, tcp_err);
          return;
        }
      };
      info!("WS server waiting for connections on {}", addr);
      if reactor_io.tx.send(PollnetMessage::Connect).is_err() {
        error!("Channel died before entering serve loop!");
        return;
      }
      loop {
        tokio::select! {
            from_c_message = reactor_io.rx.recv() => {
                match from_c_message {
                    Some(PollnetMessage::Text(_msg)) => {}, // server socket ignores sends
                    _ => break
                }
            },
            new_client = listener.accept() => {
                match new_client {
                    Ok((tcp_stream, addr)) => {
                        tokio::spawn(accept_ws(tcp_stream, addr, reactor_io.tx.clone()));
                    },
                    Err(msg) => {
                        send_error(reactor_io.tx, msg);
                        break;
                    }
                }
            },
        };
      }
    });

    let socket = Box::new(PollnetSocket {
      io: Some(host_io),
      status: SocketStatus::Opening,
      data: None,
      last_client_handle: SocketHandle::null(),
    });
    self.sockets.insert(socket)
  }
}
