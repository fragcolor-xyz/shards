/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2019 Fragcolor Pte. Ltd. */

#include "shards.h"
#include <optional>
#pragma clang attribute push(__attribute__((no_sanitize("undefined"))), apply_to = function)
#include <boost/container/stable_vector.hpp>
#pragma clang attribute pop

// ASIO must go first!!
#include <boost/asio.hpp>

#include "../runtime.hpp"
#include "foundation.hpp"
#include "shards.hpp"
#include "shardwrapper.hpp"
#include "shared.hpp"
#include "utility.hpp"
#include <boost/lockfree/queue.hpp>
#include <functional>
#include <memory>
#include <shared_mutex>
#include <stdint.h>
#include <thread>
#include <unordered_map>
#include <utility>
#include <ikcp.h>

using boost::asio::ip::udp;

namespace shards {
namespace Network {
constexpr uint32_t PeerCC = 'netP';

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
    boost::asio::post(_io_context, [this]() {
      // allow end/thread exit
      _io_context.stop();
    });
    _io_context_thread.join();
  }
};

struct NetworkPeer {
  NetworkPeer() {
    kcp = ikcp_create('shrd', this);
    // set "turbo" mode
    ikcp_nodelay(kcp, 1, 10, 2, 1);
  }

  ~NetworkPeer() { ikcp_release(kcp); }

  void maybeUpdate() {
    auto now = SHClock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - _start).count();
    if (ikcp_check(kcp, ms) <= ms) {
      ikcp_update(kcp, ms);
    }
  }

  std::optional<udp::endpoint> endpoint{};
  ikcpcb *kcp = nullptr;
  Serialization des{};
  OwnedVar payload{}; // when client
  SeqVar payloads{};  // when server

  SHTime _start = SHClock::now();

  void *user = nullptr;

  // used only when Server
  std::shared_ptr<SHWire> wire;
  std::optional<entt::connection> onStopConnection;
};

struct NetworkBase {
  static inline Type PeerInfo{{SHType::Object, {.object = {.vendorId = CoreCC, .typeId = PeerCC}}}};
  SHVar *_peerVar = nullptr;

  ParamVar _addr{Var("localhost")};
  ParamVar _port{Var(9191)};

  std::shared_ptr<entt::any> _contextStorage;
  NetworkContext *_context = nullptr;

  std::optional<udp::socket> _socket;

  void cleanup() {
    if (_context) {
      auto &io_context = _context->_io_context;

      // defer all in the context or we will crash!
      if (_socket) {
        boost::asio::post(io_context, [&]() {
          _socket->close();
          _socket.reset();
        });
      }
    }

    _contextStorage.reset();
    _context = nullptr;

    // clean context vars
    if (_peerVar) {
      releaseVariable(_peerVar);
      _peerVar = nullptr;
    }

    _addr.cleanup();
    _port.cleanup();
  }

  void warmup(SHContext *context) {
    auto networkContext = context->anyStorage["Network.Context"].lock();
    if (!networkContext) {
      _contextStorage = std::make_shared<entt::any>(std::in_place_type_t<NetworkContext>{});
      context->anyStorage["Network.Context"] = _contextStorage;
      auto anyPtr = _contextStorage.get();
      _context = &entt::any_cast<NetworkContext &>(*anyPtr);
    } else {
      _contextStorage = networkContext;
      _context = entt::any_cast<NetworkContext *>(*networkContext);
    }

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
    char *buffer;
    size_t offset;
    size_t max;

    Writer(char *buf, size_t size) : buffer(buf), offset(0), max(size) {}

    void operator()(const uint8_t *buf, size_t size) {
      auto newOffset = offset + size;
      if (newOffset > max) {
        throw ActivationError("Overflow requested");
      }
      memcpy(buffer + offset, buf, size);
      offset = newOffset;
    }
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

struct Server : public NetworkBase {
  std::shared_mutex peersMutex;
  udp::endpoint _sender;

  std::unordered_map<udp::endpoint, std::shared_ptr<NetworkPeer>> _end2Peer;
  std::unordered_map<const SHWire *, std::weak_ptr<NetworkPeer>> _wire2Peer;
  std::unique_ptr<WireDoppelgangerPool<NetworkPeer>> _pool;
  OwnedVar _handlerMaster{};

  static inline ParamsInfo params = ParamsInfo(
      ParamsInfo::Param("Address", SHCCSTR("The local bind address or the remote address."), CoreInfo::StringOrStringVar),
      ParamsInfo::Param("Port", SHCCSTR("The port to bind if server or to connect to if client."), CoreInfo::IntOrIntVar),
      ParamsInfo::Param(
          "Handler", SHCCSTR("The wire to spawn for each new peer that connects, stopping that wire will break the connection."),
          CoreInfo::WireOrNone));

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
      if (value.valueType == SHType::Wire)
        _pool.reset(new WireDoppelgangerPool<NetworkPeer>(_handlerMaster.payload.wireValue));
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
      return _handlerMaster;
    default:
      return Var::Empty;
    }
  }

  SHTypeInfo compose(SHInstanceData &data) {
    // inject our special context vars
    _sharedCopy = data.shared;
    auto endpointInfo = ExposedInfo::Variable("Network.Peer", SHCCSTR("The active peer."), SHTypeInfo(PeerInfo));
    _sharedCopy.push_back(endpointInfo);
    return data.inputType;
  }

  void warmup(SHContext *context) {
    if (!_pool) {
      throw ComposeError("Peer wires pool not valid!");
    }

    _contextCopy = context;

    NetworkBase::warmup(context);
  }

  void cleanup() {
    if (_pool)
      _pool->stopAll();

    _contextCopy.reset();

    NetworkBase::cleanup();
  }

  static int udp_output(const char *buf, int len, ikcpcb *kcp, void *user) {
    NetworkPeer *p = (NetworkPeer *)user;
    Server *s = (Server *)p->user;
    s->_socket->async_send_to(boost::asio::buffer(buf, len), *p->endpoint,
                              [](boost::system::error_code ec, std::size_t bytes_sent) {
                                if (ec) {
                                  std::cout << "Error sending: " << ec.message() << std::endl;
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
      data.shared = server._sharedCopy;
      data.wire = wire;
      wire->mesh = (*server._contextCopy)->main->mesh;
      auto res = composeWire(
          wire,
          [](const struct Shard *errorShard, const char *errorTxt, SHBool nonfatalWarning, void *userData) {
            if (!nonfatalWarning) {
              SHLOG_ERROR(errorTxt);
              throw ActivationError("Http.Server handler wire compose failed");
            } else {
              SHLOG_WARNING(errorTxt);
            }
          },
          nullptr, data);
      arrayFree(res.exposedInfo);
      arrayFree(res.requiredInfo);
    }
  };

  void wireOnStop(const SHWire::OnStopEvent &e) {
    SHLOG_TRACE("Wire {} stopped", e.wire->name);

    std::shared_lock<std::shared_mutex> lock(peersMutex);
    auto container = _wire2Peer[e.wire].lock();
    _pool->release(container);
    lock.unlock();

    std::unique_lock<std::shared_mutex> lock2(peersMutex);
    _end2Peer.erase(*container->endpoint);
  }

  void do_receive() {
    thread_local std::vector<uint8_t> recv_buffer(0xFFFF);
    _socket->async_receive_from(
        boost::asio::buffer(recv_buffer.data(), recv_buffer.size()), _sender,
        [this](boost::system::error_code ec, std::size_t bytes_recvd) {
          if (!ec && bytes_recvd > 0) {
            ikcpcb *kcp = nullptr;
            std::shared_lock<std::shared_mutex> lock(peersMutex);
            auto it = _end2Peer.find(_sender);
            if (it == _end2Peer.end()) {
              // new peer
              lock.unlock();

              // we write so hard lock this
              std::unique_lock<std::shared_mutex> lock(peersMutex);

              // new peer
              auto peer = _pool->acquire(_composer, (void *)0);
              _end2Peer[_sender] = peer;
              peer->endpoint = _sender;
              peer->user = this;
              peer->kcp->user = &peer;
              peer->kcp->output = &Server::udp_output;
              SHLOG_DEBUG("Added new peer: {} port: {}", peer->endpoint->address().to_string(), peer->endpoint->port());

              // Assume that we recycle containers so the connection might already exist!
              if (!peer->onStopConnection) {
                _wire2Peer[peer->wire.get()] = peer;
                peer->onStopConnection = peer->wire->dispatcher.sink<SHWire::OnStopEvent>().connect<&Server::wireOnStop>(this);
              }

              kcp = peer->kcp;

              auto mesh = (*_contextCopy)->main->mesh.lock();
              mesh->schedule(peer->wire, peer->payloads, false);
            } else {
              // existing peer
              kcp = it->second->kcp;

              lock.unlock();
            }

            ikcp_input(kcp, (char *)recv_buffer.data(), bytes_recvd);

            // keep receiving
            do_receive();
          }
        });
  }

  std::vector<uint8_t> _buffer;

  IterableExposedInfo _sharedCopy;
  std::optional<SHContext *> _contextCopy;
  Composer _composer{*this};

  SHVar activate(SHContext *context, const SHVar &input) {
    assert(_context);
    auto &io_context = _context->_io_context;

    if (!_socket) {
      // first activation, let's init
      _socket.emplace(io_context, udp::endpoint(udp::v4(), _port.get().payload.intValue));

      // start receiving
      boost::asio::post(io_context, [this]() { do_receive(); });
    }

    std::shared_lock<std::shared_mutex> lock(peersMutex);

    for (auto &[end, peer] : _end2Peer) {
      peer->maybeUpdate();

      setPeer(context, *peer);

      auto nextSize = ikcp_peeksize(peer->kcp);
      while (nextSize > 0) {
        _buffer.resize(nextSize);
        auto size = ikcp_recv(peer->kcp, (char *)_buffer.data(), nextSize);
        assert(size == nextSize);

        // deserialize from buffer on top of the vector of payloads, wires might consume them out of band
        Reader r((char *)_buffer.data(), nextSize);
        peer->des.reset();
        auto idx = peer->payloads.size();
        peer->payloads.resize(idx + 1);
        peer->des.deserialize(r, peer->payloads[idx]);

        peer->wire->currentInput = peer->payloads[idx];

        nextSize = ikcp_peeksize(peer->kcp);
      }
    }

    return input;
  }
};

struct Client : public NetworkBase {
  NetworkPeer _peer;
  ShardsVar _blks{};

  static inline ParamsInfo params = ParamsInfo(
      ParamsInfo::Param("Address", SHCCSTR("The local bind address or the remote address."), CoreInfo::StringOrStringVar),
      ParamsInfo::Param("Port", SHCCSTR("The port to bind if server or to connect to if client."), CoreInfo::IntOrIntVar),
      ParamsInfo::Param("Handler", SHCCSTR("The flow to execute when a packet is received."), CoreInfo::ShardsOrNone));

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
    c->_socket->async_send_to(boost::asio::buffer(buf, len), c->_server,
                              [](boost::system::error_code ec, std::size_t bytes_sent) {
                                if (ec) {
                                  std::cout << "Error sending: " << ec.message() << std::endl;
                                }
                              });
    return 0;
  }

  void setup() {
    // new peer
    _peer.kcp->user = this;
    _peer.kcp->output = &Client::udp_output;
  }

  void do_receive() {
    thread_local std::vector<uint8_t> recv_buffer(0xFFFF);
    _socket->async_receive_from(boost::asio::buffer(recv_buffer.data(), recv_buffer.size()), _server,
                                [this](boost::system::error_code ec, std::size_t bytes_recvd) {
                                  if (!ec && bytes_recvd > 0) {
                                    ikcp_input(_peer.kcp, (char *)recv_buffer.data(), bytes_recvd);

                                    // keep receiving
                                    do_receive();
                                  }
                                });
  }

  std::vector<uint8_t> _buffer;

  SHTypeInfo compose(SHInstanceData &data) {
    // inject our special context vars
    auto endpointInfo = ExposedInfo::Variable("Network.Peer", SHCCSTR("The active peer."), SHTypeInfo(PeerInfo));
    shards::arrayPush(data.shared, endpointInfo);
    _blks.compose(data);
    return data.inputType;
  }

  void cleanup() {
    NetworkBase::cleanup();
    _blks.cleanup();
  }

  void warmup(SHContext *context) {
    NetworkBase::warmup(context);
    _blks.warmup(context);
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    assert(_context);
    auto &io_context = _context->_io_context;

    if (!_socket) {
      // first activation, let's init
      _socket.emplace(io_context, udp::endpoint(udp::v4(), 0));

      boost::asio::io_service io_service;
      udp::resolver resolver(io_service);
      auto sport = std::to_string(_port.get().payload.intValue);
      udp::resolver::query query(udp::v4(), _addr.get().payload.stringValue, sport);
      _server = *resolver.resolve(query);
      _peer.endpoint = _server;

      // start receiving
      boost::asio::post(io_context, [this]() { do_receive(); });
    }

    setPeer(context, _peer);

    _peer.maybeUpdate();

    auto nextSize = ikcp_peeksize(_peer.kcp);
    if (nextSize > 0) {
      _buffer.resize(nextSize);
      auto size = ikcp_recv(_peer.kcp, (char *)_buffer.data(), nextSize);
      assert(size == nextSize);

      // deserialize from buffer
      Reader r((char *)_buffer.data(), nextSize);
      _peer.des.reset();
      _peer.des.deserialize(r, _peer.payload);

      SHVar output{};
      activateShards(SHVar(_blks).payload.seqValue, context, _peer.payload, output);
    }

    return input;
  }

  udp::endpoint _server;
};

struct Send {
  // Must take an optional seq of SocketData, to be used properly by server
  // This way we get also a easy and nice broadcast

  ThreadShared<std::array<char, 0xFFFF>> _send_buffer;

  SHVar *_peerVar = nullptr;

  std::shared_ptr<entt::any> _contextStorage;
  NetworkContext *_context = nullptr;

  void cleanup() {
    _contextStorage.reset();
    _context = nullptr;

    // clean context vars
    if (_peerVar) {
      releaseVariable(_peerVar);
      _peerVar = nullptr;
    }
  }

  void warmup(SHContext *context) {
    auto networkContext = context->anyStorage["Network.Context"].lock();
    if (!networkContext) {
      throw WarmupError("Network.Context not set");
    } else {
      _contextStorage = networkContext;
      auto anyPtr = networkContext.get();
      _context = &entt::any_cast<NetworkContext &>(*anyPtr);
    }
  }

  NetworkPeer *getPeer(SHContext *context) {
    if (!_peerVar) {
      _peerVar = referenceVariable(context, "Network.Peer");
    }
    assert(_peerVar->payload.objectVendorId == CoreCC);
    assert(_peerVar->payload.objectTypeId == PeerCC);
    return reinterpret_cast<NetworkPeer *>(_peerVar->payload.objectValue);
  }

  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return CoreInfo::AnyType; }

  Serialization serializer;

  SHVar activate(SHContext *context, const SHVar &input) {
    auto peer = getPeer(context);
    NetworkBase::Writer w(&_send_buffer().front(), _send_buffer().size());
    serializer.reset();
    auto size = serializer.serialize(input, w);
    ikcp_send(peer->kcp, (char *)_send_buffer().data(), size);
    return input;
  }
};
}; // namespace Network

void registerNetworkShards() {
  using namespace Network;
  REGISTER_SHARD("Network.Server", Server);
  REGISTER_SHARD("Network.Client", Client);
  REGISTER_SHARD("Network.Send", Send);
}
}; // namespace shards
