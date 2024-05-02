/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2019 Fragcolor Pte. Ltd. */

#include "network.hpp"

#include <shards/core/shared.hpp>
#include <shards/core/foundation.hpp>
#include <shards/shards.hpp>
#include <shards/shardwrapper.hpp>
#include <shards/core/shared.hpp>
#include <shards/core/wire_doppelganger_pool.hpp>
#include <shards/core/serialization.hpp>
#include <shards/utility.hpp>
#include <optional>
#include <boost/lockfree/queue.hpp>
#include <functional>
#include <memory>
#include <shared_mutex>
#include <stdint.h>
#include <thread>
#include <unordered_map>
#include <utility>

#include "core/async.hpp"

#pragma clang attribute push(__attribute__((no_sanitize("undefined"))), apply_to = function)
#include <ikcp.h>
#pragma clang attribute pop

#define IKCP_MAX_PKT_SIZE 10000

namespace shards {
namespace Network {
constexpr uint32_t PeerCC = 'netP';
constexpr uint32_t ServerCC = 'netS';

struct NetworkContext {
  boost::asio::io_context _io_context;
  std::thread _io_context_thread;

  NetworkContext() {
    _io_context_thread = std::thread([this] {
      // Force run to run even without work
      boost::asio::executor_work_guard<boost::asio::io_context::executor_type> g = boost::asio::make_work_guard(_io_context);
      try {
        SHLOG_DEBUG("Boost asio context running...");
        _io_context.run();
      } catch (...) {
        SHLOG_ERROR("Boost asio context run failed.");
      }
      SHLOG_DEBUG("Boost asio context exiting...");
    });
  }

  ~NetworkContext() {
    SHLOG_TRACE("NetworkContext dtor");
    _io_context.stop(); // internally it will lock and send stop to all threads
    if (_io_context_thread.joinable())
      _io_context_thread.join();
  }
};

struct NetworkPeer;

struct NetworkBase {
  static inline Type PeerInfo{{SHType::Object, {.object = {.vendorId = CoreCC, .typeId = PeerCC}}}};
  SHVar *_peerVar = nullptr;

  ParamVar _addr{Var("localhost")};
  ParamVar _port{Var(9191)};

  AnyStorage<NetworkContext> _sharedNetworkContext;

  std::optional<udp::socket> _socket;
  std::mutex _socketMutex;

  ExposedInfo _required;
  SHTypeInfo compose(const SHInstanceData &data) {
    _required.clear();
    collectRequiredVariables(data.shared, _required, (SHVar &)_addr);
    collectRequiredVariables(data.shared, _required, (SHVar &)_port);
    return data.inputType;
  }

  SHExposedTypesInfo requiredVariables() { return SHExposedTypesInfo(_required); }

  void cleanup(SHContext *context) {
    if (_sharedNetworkContext) {
      auto &io_context = _sharedNetworkContext->_io_context;

      // defer all in the context or we will crash!
      if (_socket) {
        std::scoped_lock<std::mutex> l(_socketMutex);

        struct Container {
          udp::socket socket;
          Container(udp::socket &&s) : socket(std::move(s)) {}
        };

        boost::asio::post(io_context, [socket_ = std::make_unique<Container>(std::move(_socket.value()))]() {
          SHLOG_TRACE("Closing socket");
          socket_->socket.close();
        });

        _socket.reset();
      }
    }

    // clean context vars
    if (_peerVar) {
      releaseVariable(_peerVar);
      _peerVar = nullptr;
    }

    _addr.cleanup();
    _port.cleanup();
  }

  void warmup(SHContext *context) {
    auto mesh = context->main->mesh.lock();
    _sharedNetworkContext = getOrCreateAnyStorage<NetworkContext>(mesh.get(), "Network.Context");

    _addr.warmup(context);
    _port.warmup(context);
  }

  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }

  static SHTypesInfo outputTypes() { return CoreInfo::AnyType; }

  struct Reader {
    char *buffer;
    size_t offset;
    size_t max;

    Reader(char *buf, size_t size) : buffer(buf), offset(0), max(size) {}

    void operator()(uint8_t *buf, size_t size) {
      auto newOffset = offset + size;
      if (newOffset > max) {
        throw ActivationError("Overflow requested");
      }
      memcpy(buf, buffer + offset, size);
      offset = newOffset;
    }
  };

  struct Writer {
    // let's make this growable on demand
    std::vector<uint8_t> buffer;
    size_t offset;

    Writer() {}

    void operator()(const uint8_t *buf, size_t size) {
      auto newOffset = offset + size;
      // resize buffer if needed but it (*2) so to avoid too many resizes
      if (newOffset > buffer.size()) {
        buffer.resize(newOffset * 2);
      }
      memcpy(buffer.data() + offset, buf, size);
      offset = newOffset;
    }

    void reset() {
      offset = 4; // we reserve 4 bytes for size
      buffer.resize(offset);
    }

    void finalize() {
      // write size
      *(uint32_t *)buffer.data() = offset;
    }

    // expose data and size
    char *data() { return (char *)buffer.data(); }
    size_t size() { return offset; }
  };

  void setPeer(SHContext *context, NetworkPeer &peer) {
    if (!_peerVar) {
      _peerVar = referenceVariable(context, "Network.Peer");
    }
    auto rc = _peerVar->refcount;
    auto flags = _peerVar->flags;
    *_peerVar = Var::Object(&peer, CoreCC, PeerCC);
    _peerVar->refcount = rc;
    _peerVar->flags = flags;
  }
};

struct NetworkPeer {
  NetworkPeer() {}

  ~NetworkPeer() {
    if (kcp) {
      ikcp_release(kcp);
      kcp = nullptr;
    }
  }

  bool tryReceive(SHContext *context) {
    std::scoped_lock peerLock(mutex);

    if (networkError) {
      decltype(networkError) e;
      std::swap(e, networkError);
      throw std::runtime_error(fmt::format("Failed to receive: {}", e->message()));
    }

    auto now = SHClock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - _start).count();
    ikcp_update(kcp, ms);

    // Initialize variables for buffer management.
    size_t offset = recvBuffer.size();
    auto nextChunkSize = ikcp_peeksize(kcp);
    // if (nextChunkSize != -1)
    //   SHLOG_TRACE("nextChunkSize: {}, endpoint: {} port: {}, offset: {}", nextChunkSize, endpoint->address().to_string(),
    //               endpoint->port(), offset);

    // Loop to receive all available chunks.
    while (nextChunkSize > 0) {
      // Resize the buffer to hold the incoming chunk.
      recvBuffer.resize(offset + nextChunkSize);

      // Receive the chunk.
      auto receivedSize = ikcp_recv(kcp, (char *)recvBuffer.data() + offset, nextChunkSize);
      shassert(receivedSize == nextChunkSize && "ikcp_recv returned unexpected size");

      // If this is the first chunk, read the expected total size from its prefix.
      if (offset == 0) {
        expectedSize = *(uint32_t *)recvBuffer.data();
        // reserve in advance as well or we will have to resize MANY TIMES later, which is slow
        recvBuffer.reserve(expectedSize);
      }

      // Check if the current buffer matches the expected size.
      if (recvBuffer.size() == expectedSize) {
        // SHLOG_TRACE("Received full packet, endpoint: {} port: {}, size: {}", endpoint->address().to_string(), endpoint->port(),
        //             expectedSize);
        return true;
      } else {
        // We expect another chunk; update the offset.
        offset = recvBuffer.size();
        nextChunkSize = ikcp_peeksize(kcp);
        // SHLOG_TRACE("nextChunkSize (2): {}, endpoint: {} port: {}, offset: {}, expected: {}", nextChunkSize,
        //             endpoint->address().to_string(), endpoint->port(), offset, expectedSize);
      }
    }

    return false;
  }

  void endReceive() {
    // Reset the buffer.
    recvBuffer.clear();
    expectedSize = 0;
  }

  static inline thread_local NetworkBase::Writer _sendWriter;

  Serialization serializer;

  void sendVar(const SHVar &input) {
    _sendWriter.reset();
    serializer.reset();
    serializer.serialize(input, _sendWriter);
    _sendWriter.finalize();
    auto size = _sendWriter.size();

    std::scoped_lock lock(mutex); // prevent concurrent sends

    if (size > IKCP_MAX_PKT_SIZE) {
      // send in chunks
      size_t chunks = size / IKCP_MAX_PKT_SIZE;
      size_t remaining = size % IKCP_MAX_PKT_SIZE;
      auto ptr = _sendWriter.data();
      for (size_t i = 0; i < chunks; ++i) {
        auto err = ikcp_send(kcp, ptr, IKCP_MAX_PKT_SIZE);
        if (err < 0) {
          SHLOG_ERROR("ikcp_send error: {}", err);
          throw ActivationError("ikcp_send error");
        }
        ptr += IKCP_MAX_PKT_SIZE;
      }
      if (remaining > 0) {
        auto err = ikcp_send(kcp, ptr, remaining);
        if (err < 0) {
          SHLOG_ERROR("ikcp_send error: {}", err);
          throw ActivationError("ikcp_send error");
        }
      }
    } else {
      // directly send as it's small enough
      auto err = ikcp_send(kcp, _sendWriter.data(), _sendWriter.size());
      if (err < 0) {
        SHLOG_ERROR("ikcp_send error: {}", err);
        throw ActivationError("ikcp_send error");
      }
    }
  }

  void reset() {
    if (kcp)
      ikcp_release(kcp);

    kcp = ikcp_create('shrd', this);

    // set "turbo" mode
    ikcp_nodelay(kcp, 1, 10, 2, 1);

    _start = SHClock::now();
    _lastContact = SHClock::now();
    disconnected = false;
    networkError.reset();
    recvBuffer.clear();
    expectedSize = 0;
  }

  std::optional<udp::endpoint> endpoint{};
  ikcpcb *kcp = nullptr;
  Serialization des{};
  OwnedVar payload{};

  SHTime _start = SHClock::now();
  std::atomic<SHTime> _lastContact = SHClock::now();

  void *user = nullptr;

  // used only when Server
  std::shared_ptr<SHWire> wire;
  std::optional<entt::connection> onStopConnection;

  std::mutex mutex;

  std::vector<uint8_t> recvBuffer;
  uint32_t expectedSize = 0;

  std::optional<boost::system::error_code> networkError;

  std::atomic_bool disconnected = false;
};

struct Server : public NetworkBase {
  std::shared_mutex peersMutex;
  udp::endpoint _sender;

  std::unordered_map<udp::endpoint, NetworkPeer *> _end2Peer;
  std::unordered_map<const SHWire *, NetworkPeer *> _wire2Peer;
  std::unique_ptr<WireDoppelgangerPool<NetworkPeer>> _pool;
  OwnedVar _handlerMaster{};

  SHVar *_serverVar{nullptr};

  bool _running = false;

  float _timeoutSecs = 30.0f;

  ShardsVar _disconnectionHandler{};

  static inline Parameters params{
      {"Address", SHCCSTR("The local bind address or the remote address."), {CoreInfo::StringOrStringVar}},
      {"Port", SHCCSTR("The port to bind if server or to connect to if client."), {CoreInfo::IntOrIntVar}},
      {"Handler",
       SHCCSTR("The wire to spawn for each new peer that connects, stopping that wire will break the connection."),
       {CoreInfo::WireOrNone}},
      {"Timeout",
       SHCCSTR("The timeout in seconds after which a peer will be disconnected if there is no network activity."),
       {CoreInfo::FloatType}},
      {"OnDisconnect",
       SHCCSTR("The flow to execute when a peer disconnects, The Peer ID will be the input."),
       {CoreInfo::ShardsOrNone}}};

  static SHParametersInfo parameters() { return SHParametersInfo(params); }

  void setParam(int index, const SHVar &value) {
    switch (index) {
    case 0:
      _addr = value;
      break;
    case 1:
      _port = value;
      break;
    case 2:
      _handlerMaster = value;
      break;
    case 3:
      _timeoutSecs = value.payload.floatValue;
      break;
    case 4:
      _disconnectionHandler = value;
    default:
      break;
    }
  }

  SHVar getParam(int index) {
    switch (index) {
    case 0:
      return _addr;
    case 1:
      return _port;
    case 2:
      return _handlerMaster;
    case 3:
      return Var(_timeoutSecs);
    case 4:
      return _disconnectionHandler;
    default:
      return Var::Empty;
    }
  }

  SHTypeInfo compose(const SHInstanceData &data) {
    if (_handlerMaster.valueType == SHType::Wire)
      _pool.reset(new WireDoppelgangerPool<NetworkPeer>(_handlerMaster.payload.wireValue));

    // compose first with data the _disconnectionHandler if any
    if (_disconnectionHandler) {
      SHInstanceData dataCopy = data;
      dataCopy.inputType = CoreInfo::IntType;
      _disconnectionHandler.compose(dataCopy);
    }

    // inject our special context vars
    _sharedCopy = ExposedInfo(data.shared);
    auto endpointInfo = ExposedInfo::Variable("Network.Peer", SHCCSTR("The active peer."), SHTypeInfo(PeerInfo));
    _sharedCopy.push_back(endpointInfo);

    return NetworkBase::compose(data);
  }

  void gcWires(SHContext *context) {
    if (!_stopWireQueue.empty()) {
      SHWire *toStop{};
      while (_stopWireQueue.pop(toStop)) {
        SHLOG_TRACE("GC-ing wire {}", toStop->name);
        DEFER({ SHLOG_TRACE("GC-ed wire {}", toStop->name); });

        // read lock this
        std::shared_lock<std::shared_mutex> lock(peersMutex);
        auto it = _wire2Peer.find(toStop);
        if (it == _wire2Peer.end())
          continue; // Wire is not managed by this server
        NetworkPeer *container = it->second;
        lock.unlock();

        SHLOG_TRACE("Clearing endpoint {}", container->endpoint->address().to_string());

        container->disconnected = true;

        // call this before cleanup
        if (_disconnectionHandler) {
          Var peerId(*reinterpret_cast<int64_t *>(&toStop->id));
          SHVar output{};
          _disconnectionHandler.activate(context, peerId, output);
        }

        // ensure cleanup is called early here
        auto wireState = toStop->state.load();
        if (wireState != SHWire::State::Failed && wireState != SHWire::State::Stopped) {
          // in the above cases cleanup already happened
          const_cast<SHWire *>(toStop)->cleanup();
        } else {
          SHLOG_TRACE("Wire {} already stopped, state: {}", toStop->name, wireState);
        }

        // now fire events
        if (_contextCopy) {
          OnPeerDisconnected event{
              .endpoint = *container->endpoint,
              .wire = container->wire,
          };
          (*_contextCopy)->main->mesh.lock()->dispatcher.trigger(std::move(event));
        }

        // write lock it now
        std::scoped_lock<std::shared_mutex> lock2(peersMutex);
        _end2Peer.erase(*container->endpoint);
        _pool->release(container);
      }
    }
  }

  void warmup(SHContext *context) {
    if (!_pool) {
      throw ComposeError("Peer wires pool not valid!");
    }

    if (_disconnectionHandler)
      _disconnectionHandler.warmup(context);

    _contextCopy = context;

    NetworkBase::warmup(context);

    setServer(context, this);

    _running = true;
  }

  void cleanup(SHContext *context) {
    _running = false;

    if (_serverVar) {
      releaseVariable(_serverVar);
      _serverVar = nullptr;
    }

    if (_pool) {
      SHLOG_TRACE("Stopping all wires");
      _pool->stopAll();
    } else {
      SHLOG_TRACE("No pool to stop");
    }

    _contextCopy.reset();

    NetworkBase::cleanup(context);

    gcWires(context);

    if (_disconnectionHandler)
      _disconnectionHandler.cleanup(context);
  }

  static int udp_output(const char *buf, int len, ikcpcb *kcp, void *user) {
    NetworkPeer *p = (NetworkPeer *)user;
    Server *s = (Server *)p->user;

    std::scoped_lock<std::mutex> l(s->_socketMutex); // not ideal but for now we gotta do it

    s->_socket->async_send_to(boost::asio::buffer(buf, len), *p->endpoint,
                              [](boost::system::error_code ec, std::size_t bytes_sent) {
                                if (ec) {
                                  // ignore flow-control No buffer space available
                                  if (ec != boost::asio::error::no_buffer_space && ec != boost::asio::error::would_block &&
                                      ec != boost::asio::error::try_again) {
                                    SHLOG_ERROR("Error sending: {}", ec.message());
                                  } else {
                                    SHLOG_DEBUG("Error sending (ignored): {}", ec.message());
                                  }
                                }
                              });
    return 0;
  }

  struct Composer {
    Server &server;

    void compose(SHWire *wire, void *nothing, bool recycling) {
      if (recycling)
        return;

      SHInstanceData data{};
      data.inputType = CoreInfo::AnySeqType;
      data.shared = SHExposedTypesInfo(server._sharedCopy);
      data.wire = wire;
      wire->mesh = (*server._contextCopy)->main->mesh;
      auto res = composeWire(wire, data);
      arrayFree(res.exposedInfo);
      arrayFree(res.requiredInfo);
    }
  };

  boost::lockfree::queue<const SHWire *> _stopWireQueue{16};

  void wireOnStop(const SHWire::OnStopEvent &e) {
    SHLOG_TRACE("Wire {} stopped", e.wire->name);

    _stopWireQueue.push(e.wire);
  }

  void do_receive() {
    thread_local std::vector<uint8_t> recv_buffer(0xFFFF);

    std::scoped_lock<std::mutex> l(_socketMutex); // not ideal but for now we gotta do it

    _socket->async_receive_from(
        boost::asio::buffer(recv_buffer.data(), recv_buffer.size()), _sender,
        [this](boost::system::error_code ec, std::size_t bytes_recvd) {
          if (!ec && bytes_recvd > 0) {
            NetworkPeer *currentPeer = nullptr;
            std::shared_lock<std::shared_mutex> lock(peersMutex);
            auto it = _end2Peer.find(_sender);
            if (it == _end2Peer.end()) {
              SHLOG_TRACE("Received packet from unknown peer: {} port: {}", _sender.address().to_string(), _sender.port());

              // new peer
              lock.unlock();

              // we write so hard lock this
              std::scoped_lock<std::shared_mutex> lock(peersMutex);

              // new peer
              try {
                auto peer = _pool->acquire(_composer, (void *)0);
                peer->reset();
                _end2Peer[_sender] = peer;
                peer->endpoint = _sender;
                peer->user = this;
                peer->kcp->user = peer;
                peer->kcp->output = &Server::udp_output;
                SHLOG_DEBUG("Added new peer: {} port: {}", peer->endpoint->address().to_string(), peer->endpoint->port());

                // Assume that we recycle containers so the connection might already exist!
                if (!peer->onStopConnection) {
                  _wire2Peer[peer->wire.get()] = peer;
                  peer->onStopConnection =
                      peer->wire->mesh.lock()->dispatcher.sink<SHWire::OnStopEvent>().connect<&Server::wireOnStop>(this);
                }

                // set wire ID, in order for Events to be properly routed
                // for now we just use ptr as ID, until it causes problems
                peer->wire->id = reinterpret_cast<entt::id_type>(peer);

                currentPeer = peer;
              } catch (std::exception &e) {
                SHLOG_ERROR("Error acquiring peer: {}", e.what());

                // keep receiving
                if (_socket && _running)
                  return do_receive();
              }
            } else {
              // SHLOG_TRACE("Received packet from known peer: {} port: {}", _sender.address().to_string(), _sender.port());

              // existing peer
              currentPeer = it->second;

              lock.unlock();
            }

            {
              std::scoped_lock pLock(currentPeer->mutex);

              auto err = ikcp_input(currentPeer->kcp, (char *)recv_buffer.data(), bytes_recvd);
              // SHLOG_TRACE("ikcp_input: {}, peer: {} port: {}, size: {}", err, _sender.address().to_string(), _sender.port(),
              //             bytes_recvd);
              if (err < 0) {
                SHLOG_ERROR("Error ikcp_input: {}, peer: {} port: {}", err, _sender.address().to_string(), _sender.port());
                _stopWireQueue.push(currentPeer->wire.get());
              }

              currentPeer->_lastContact = SHClock::now();
            }

            // keep receiving
            if (_socket && _running) {
              return do_receive();
            } else {
              SHLOG_DEBUG("Socket closed, stopping receive loop");
            }
          } else {
            if (ec == boost::asio::error::operation_aborted) {
              // we likely have invalid data under the hood, let's just ignore it
              SHLOG_DEBUG("Operation aborted");
              return;
            } else if (ec == boost::asio::error::no_buffer_space || ec == boost::asio::error::would_block ||
                       ec == boost::asio::error::try_again) {
              SHLOG_DEBUG("Ignored error while receiving: {}", ec.message());
              return do_receive();
            }

            SHLOG_DEBUG("Error receiving: {}, peer: {} port: {}", ec.message(), _sender.address().to_string(), _sender.port());

            std::shared_lock<std::shared_mutex> lock(peersMutex);
            auto it = _end2Peer.find(_sender);
            if (it != _end2Peer.end()) {
              SHLOG_TRACE("Removing peer: {} port: {}", _sender.address().to_string(), _sender.port());
              _stopWireQueue.push(it->second->wire.get());
            }

            // keep receiving
            if (_socket && _running) {
              return do_receive();
            } else {
              SHLOG_DEBUG("Socket closed, stopping receive loop");
            }
          }
        });
  }

  ExposedInfo _sharedCopy;
  std::optional<SHContext *> _contextCopy;
  Composer _composer{*this};

  void setServer(SHContext *context, Server *peer) {
    if (!_serverVar) {
      _serverVar = referenceVariable(context, "Network.Server");
    }
    auto rc = _serverVar->refcount;
    auto flags = _serverVar->flags;
    *_serverVar = Var::Object(peer, CoreCC, ServerCC);
    _serverVar->refcount = rc;
    _serverVar->flags = flags;
  }

  SHExposedTypesInfo exposedVariables() {
    static std::array<SHExposedTypeInfo, 1> exposing;
    exposing[0].name = "Network.Server";
    exposing[0].help = SHCCSTR("The exposed server.");
    exposing[0].exposedType = ServerType;
    exposing[0].isProtected = true;
    return {exposing.data(), 1, 0};
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    assert(_sharedNetworkContext);
    auto &io_context = _sharedNetworkContext->_io_context;

    if (!_socket) {
      // first activation, let's init
      _socket.emplace(io_context);
      _socket->open(udp::v4());
      _socket->set_option(boost::asio::ip::udp::socket::reuse_address(true));
      _socket->set_option(boost::asio::socket_base::send_buffer_size(65536));
      _socket->set_option(boost::asio::socket_base::receive_buffer_size(65536));
      _socket->bind(udp::endpoint(udp::v4(), _port.get().payload.intValue));

      // start receiving
      boost::asio::post(io_context, [this]() { do_receive(); });

      SHLOG_TRACE("Network.Server listening on port {}", _port.get().payload.intValue);
    }

    gcWires(context);

    {
      std::shared_lock<std::shared_mutex> lock(peersMutex);

      auto now = SHClock::now();

      for (auto &[end, peer] : _end2Peer) {
        if (now > (peer->_lastContact.load() + SHDuration(_timeoutSecs))) {
          SHLOG_DEBUG("Peer {}:{} timed out", peer->endpoint->address().to_string(), peer->endpoint->port());
          _stopWireQueue.push(peer->wire.get());
          continue;
        }

        setPeer(context, *peer);

        if (!peer->wire->warmedUp) {
          OnPeerConnected event{
              .endpoint = *peer->endpoint,
              .wire = peer->wire,
          };
          context->main->mesh.lock()->dispatcher.trigger(std::move(event));

          // warmup the wire after the event in order to give it a chance to set up
          peer->wire->warmup(context);
        }

        if (peer->tryReceive(context)) {
          auto &peer_ = peer; // avoid c++20 ext warning
          DEFER({
            // we need to ensure this gets called at all times
            peer_->endReceive();
          });

          if (!context->shouldContinue())
            return input;

          try {
            if (peer_->recvBuffer.size() > IKCP_MAX_PKT_SIZE) {
              // do this async as it's a big buffer
              await(
                  context,
                  [peer_]() {
                    // deserialize from buffer on top of the vector of payloads, wires might consume them out of band
                    Reader r((char *)peer_->recvBuffer.data() + 4, peer_->recvBuffer.size() - 4);
                    peer_->des.reset();
                    peer_->des.deserialize(r, peer_->payload);
                  },
                  [] {});
            } else {
              // deserialize from buffer on top of the vector of payloads, wires might consume them out of band
              Reader r((char *)peer_->recvBuffer.data() + 4, peer_->recvBuffer.size() - 4);
              peer_->des.reset();
              peer_->des.deserialize(r, peer_->payload);
            }

            // Run within the root flow
            auto runRes = runSubWire(peer->wire.get(), context, peer->payload);
            if (unlikely(runRes.state == SHRunWireOutputState::Failed || runRes.state == SHRunWireOutputState::Stopped ||
                         runRes.state == SHRunWireOutputState::Returned)) {
              stop(peer->wire.get());
            }
          } catch (std::exception &e) {
            SHLOG_ERROR("Critical errors processing peer {}: {}, disconnecting it", peer->endpoint->address().to_string(),
                        e.what());
            stop(peer->wire.get());
          }
          // Always adjust the context back to continue, peer wire might have changed it
          context->resetErrorStack();
          context->continueFlow();
        }
      }
    }

    return input;
  }

  static inline Type ServerType{{SHType::Object, {.object = {.vendorId = CoreCC, .typeId = ServerCC}}}};
};

struct Broadcast {
  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return CoreInfo::AnyType; }

  SHExposedTypesInfo requiredVariables() {
    static std::array<SHExposedTypeInfo, 1> required;
    required[0].name = "Network.Server";
    required[0].help = SHCCSTR("The required server.");
    required[0].exposedType = Server::ServerType;
    required[0].isProtected = true;
    return {required.data(), 1, 0};
  }

  SHVar *_serverVar = nullptr;

  void warmup(SHContext *context) {
    if (!_serverVar) {
      _serverVar = referenceVariable(context, "Network.Server");
    }
  }

  void cleanup(SHContext *context) {
    if (_serverVar) {
      releaseVariable(_serverVar);
      _serverVar = nullptr;
    }
  }

  static inline thread_local NetworkBase::Writer _sendWriter;
  Serialization serializer;

  std::vector<std::pair<char *, size_t>> chunks;

  SHVar activate(SHContext *context, const SHVar &input) {
    auto server = reinterpret_cast<Server *>(_serverVar->payload.objectValue);

    _sendWriter.reset();
    serializer.reset();
    serializer.serialize(input, _sendWriter);
    _sendWriter.finalize();
    auto size = _sendWriter.size();

    if (size > IKCP_MAX_PKT_SIZE) {
      // pre build a sequence of chunks of the big buffer (pointers), then go thru all peers and send
      chunks.clear();
      size_t chunkSize = IKCP_MAX_PKT_SIZE;
      size_t offset = 0;
      while (offset < size) {
        if (offset + chunkSize > size) {
          chunkSize = size - offset;
        }
        chunks.emplace_back(_sendWriter.data() + offset, chunkSize);
        offset += chunkSize;
      }

      for (auto &[end, peer] : server->_end2Peer) {
        std::scoped_lock lock(peer->mutex);
        for (auto &[chunk, chunkSize] : chunks) {
          auto err = ikcp_send(peer->kcp, chunk, chunkSize);
          if (err < 0) {
            SHLOG_ERROR("ikcp_send error: {}", err);
            throw ActivationError("ikcp_send error");
          }
        }
      }
    } else {
      // broadcast
      for (auto &[end, peer] : server->_end2Peer) {
        std::scoped_lock lock(peer->mutex);
        auto err = ikcp_send(peer->kcp, _sendWriter.data(), _sendWriter.size());
        if (err < 0) {
          SHLOG_ERROR("ikcp_send error: {}", err);
          throw ActivationError("ikcp_send error");
        }
      }
    }

    return input;
  }
};

struct Client : public NetworkBase {
  std::array<SHExposedTypeInfo, 1> _exposing;

  SHExposedTypesInfo exposedVariables() {
    _exposing[0].name = "Network.Peer";
    _exposing[0].help = SHCCSTR("The exposed peer.");
    _exposing[0].exposedType = Client::PeerType;
    _exposing[0].isProtected = true;
    return {_exposing.data(), 1, 0};
  }

  static inline Type PeerType{{SHType::Object, {.object = {.vendorId = CoreCC, .typeId = PeerCC}}}};
  static inline Type PeerObjectType = Type::VariableOf(PeerType);

  static SHTypesInfo outputTypes() { return PeerType; }

  NetworkPeer _peer;
  ShardsVar _blks{};
  udp::endpoint _server;

  static inline Parameters params{
      {"Address", SHCCSTR("The local bind address or the remote address."), {CoreInfo::StringOrStringVar}},
      {"Port", SHCCSTR("The port to bind if server or to connect to if client."), {CoreInfo::IntOrIntVar}},
      {"Handler", SHCCSTR("The flow to execute when a packet is received."), {CoreInfo::ShardsOrNone}}};

  static SHParametersInfo parameters() { return SHParametersInfo(params); }

  void setParam(int index, const SHVar &value) {
    switch (index) {
    case 0:
      _addr = value;
      break;
    case 1:
      _port = value;
      break;
    case 2:
      _blks = value;
      break;
    default:
      break;
    }
  }

  SHVar getParam(int index) {
    switch (index) {
    case 0:
      return _addr;
    case 1:
      return _port;
    case 2:
      return _blks;
    default:
      return Var::Empty;
    }
  }

  static int udp_output(const char *buf, int len, ikcpcb *kcp, void *user) {
    Client *c = (Client *)user;

    std::scoped_lock<std::mutex> l(c->_socketMutex); // not ideal but for now we gotta do it

    c->_socket->async_send_to(boost::asio::buffer(buf, len), c->_server,
                              [c](boost::system::error_code ec, std::size_t bytes_sent) {
                                if (ec) {
                                  // ignore flow-control No buffer space available
                                  if (ec != boost::asio::error::no_buffer_space && ec != boost::asio::error::would_block &&
                                      ec != boost::asio::error::try_again) {
                                    SHLOG_ERROR("Error sending: {}", ec.message());
                                    c->_peer.networkError = ec;
                                  } else {
                                    SHLOG_DEBUG("Error sending (ignored): {}", ec.message());
                                  }
                                }
                              });

    return 0;
  }

  void setup() {
    // new peer
    _peer.reset();
    _peer.kcp->user = this;
    _peer.kcp->output = &Client::udp_output;
  }

  void do_receive() {
    thread_local std::vector<uint8_t> recv_buffer(0xFFFF);

    std::scoped_lock<std::mutex> l(_socketMutex); // not ideal but for now we gotta do it

    _socket->async_receive_from(boost::asio::buffer(recv_buffer.data(), recv_buffer.size()), _server,
                                [this](boost::system::error_code ec, std::size_t bytes_recvd) {
                                  if (ec) {
                                    // certain errors are expected, ignore them
                                    if (ec == boost::asio::error::no_buffer_space || ec == boost::asio::error::would_block ||
                                        ec == boost::asio::error::try_again) {
                                      SHLOG_DEBUG("Ignored error while receiving: {}", ec.message());
                                      return do_receive(); // continue receiving
                                    } else if (ec == boost::asio::error::operation_aborted) {
                                      SHLOG_ERROR("Socket aborted receiving: {}", ec.message());
                                      // _peer might be invalid at this point!
                                    } else {
                                      SHLOG_ERROR("Error receiving: {}", ec.message());
                                      _peer.networkError = ec;
                                    }
                                  } else {
                                    if (bytes_recvd > 0) {
                                      std::scoped_lock lock(_peer.mutex);

                                      auto err = ikcp_input(_peer.kcp, (char *)recv_buffer.data(), bytes_recvd);
                                      if (err < 0) {
                                        SHLOG_ERROR("Error ikcp_input: {}");
                                      }
                                    }
                                    // keep receiving
                                    do_receive();
                                  }
                                });
  }

  SHTypeInfo compose(SHInstanceData &data) {
    NetworkBase::compose(data);
    // inject our special context vars
    auto endpointInfo = ExposedInfo::Variable("Network.Peer", SHCCSTR("The active peer."), SHTypeInfo(PeerInfo));
    shards::arrayPush(data.shared, endpointInfo);
    _blks.compose(data);
    return PeerType;
  }

  void cleanup(SHContext *context) {
    NetworkBase::cleanup(context);
    _blks.cleanup(context);
  }

  void warmup(SHContext *context) {
    NetworkBase::warmup(context);
    _blks.warmup(context);
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    assert(_sharedNetworkContext);
    auto &io_context = _sharedNetworkContext->_io_context;

    if (!_socket) {
      setup(); // reset the peer

      // first activation, let's init
      _socket.emplace(io_context, udp::endpoint(udp::v4(), 0));
      boost::asio::socket_base::send_buffer_size option_send(65536);
      boost::asio::socket_base::receive_buffer_size option_recv(65536);
      _socket->set_option(option_send);
      _socket->set_option(option_recv);

      boost::asio::io_service io_service;
      udp::resolver resolver(io_service);
      auto sport = std::to_string(_port.get().payload.intValue);
      udp::resolver::query query(udp::v4(), SHSTRING_PREFER_SHSTRVIEW(_addr.get()), sport);
      _server = *resolver.resolve(query);
      _peer.endpoint = _server;

      // start receiving
      boost::asio::post(io_context, [this]() { do_receive(); });
    }

    setPeer(context, _peer);

    if (_peer.tryReceive(context)) {
      if (!context->shouldContinue())
        return Var::Empty;

      if (_peer.recvBuffer.size() > IKCP_MAX_PKT_SIZE) {
        // do this async as it's a big buffer
        await(
            context,
            [&]() {
              // deserialize from buffer on top of the vector of payloads, wires might consume them out of band
              Reader r((char *)_peer.recvBuffer.data() + 4, _peer.recvBuffer.size() - 4);
              _peer.des.reset();
              _peer.des.deserialize(r, _peer.payload);
            },
            [] {});
      } else {
        // deserialize from buffer on top of the vector of payloads, wires might consume them out of band
        Reader r((char *)_peer.recvBuffer.data() + 4, _peer.recvBuffer.size() - 4);
        _peer.des.reset();
        _peer.des.deserialize(r, _peer.payload);
      }

      // at this point we can already cleanup the buffer
      _peer.endReceive();

      SHVar output{};
      activateShards(SHVar(_blks).payload.seqValue, context, _peer.payload, output);
      // no need to handle errors as context will eventually deal with it after activation
    }

    return Var::Object(&_peer, CoreCC, PeerCC);
  }
};

struct PeerBase {
  std::array<SHExposedTypeInfo, 1> _requiring;
  SHVar *_peerVar = nullptr;
  ParamVar _peerParam;

  void cleanup(SHContext *context) {
    // clean context vars
    if (_peerVar) {
      releaseVariable(_peerVar);
      _peerVar = nullptr;
    }

    _peerParam.cleanup();
  }

  void warmup(SHContext *context) { _peerParam.warmup(context); }

  NetworkPeer *getPeer(SHContext *context) {
    auto &fixedPeer = _peerParam.get();
    NetworkPeer *peer = nullptr;
    if (fixedPeer.valueType == SHType::Object) {
      assert(fixedPeer.payload.objectVendorId == CoreCC);
      assert(fixedPeer.payload.objectTypeId == PeerCC);
      peer = reinterpret_cast<NetworkPeer *>(fixedPeer.payload.objectValue);
    } else {
      if (!_peerVar) {
        _peerVar = referenceVariable(context, "Network.Peer");
      }
      assert(_peerVar->payload.objectVendorId == CoreCC);
      assert(_peerVar->payload.objectTypeId == PeerCC);
      peer = reinterpret_cast<NetworkPeer *>(_peerVar->payload.objectValue);
    }

    if (peer->disconnected)
      throw ActivationError("Peer is disconnected");

    return peer;
  }

  static inline Parameters params{
      {"Peer", SHCCSTR("The optional explicit peer to send packets to."), {CoreInfo::NoneType, Client::PeerObjectType}}};

  static SHParametersInfo parameters() { return SHParametersInfo(params); }

  void setParam(int index, const SHVar &value) {
    switch (index) {
    case 0:
      _peerParam = value;
      break;
    default:
      break;
    }
  }

  SHVar getParam(int index) {
    switch (index) {
    case 0:
      return _peerParam;
    default:
      return Var::Empty;
    }
  }

  SHExposedTypesInfo requiredVariables() {
    if (_peerParam.isVariable()) {
      _requiring[0].name = _peerParam.variableName();
      _requiring[0].help = SHCCSTR("The required peer.");
      _requiring[0].exposedType = Client::PeerType;
      return {_requiring.data(), 1, 0};
    } else {
      _requiring[0].name = "Network.Peer";
      _requiring[0].help = SHCCSTR("The required peer.");
      _requiring[0].exposedType = Client::PeerType;
      return {_requiring.data(), 1, 0};
    }
  }
};

struct Send : public PeerBase {
  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return CoreInfo::AnyType; }

  SHVar activate(SHContext *context, const SHVar &input) {
    auto peer = getPeer(context);

    peer->sendVar(input);

    return input;
  }
};

struct PeerID : public PeerBase {
  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return CoreInfo::IntType; }

  static inline Parameters params{
      {"Peer", SHCCSTR("The optional explicit peer to identify."), {CoreInfo::NoneType, Client::PeerObjectType}}};

  static SHParametersInfo parameters() { return SHParametersInfo(params); }

  SHVar activate(SHContext *context, const SHVar &input) {
    auto peer = getPeer(context);
    return Var(reinterpret_cast<int64_t>(peer));
  }
};

struct GetPeer : public PeerBase {
  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return Client::PeerType; }

  // override parent
  static SHParametersInfo parameters() { return SHParametersInfo{}; }
  void setParam(int index, const SHVar &value) {}
  SHVar getParam(int index) { return Var::Empty; }

  SHVar activate(SHContext *context, const SHVar &input) {
    auto peer = getPeer(context);
    return Var::Object(peer, CoreCC, PeerCC);
  }
};
}; // namespace Network
}; // namespace shards
SHARDS_REGISTER_FN(network) {
  using namespace shards::Network;
  REGISTER_SHARD("Network.Server", Server);
  REGISTER_SHARD("Network.Broadcast", Broadcast);
  REGISTER_SHARD("Network.Client", Client);
  REGISTER_SHARD("Network.Send", Send);
  REGISTER_SHARD("Network.PeerID", PeerID);
  REGISTER_SHARD("Network.Peer", GetPeer);
}
