/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2019 Fragcolor Pte. Ltd. */

#include "network.hpp"

#include <shards/core/shared.hpp>
#include <shards/core/foundation.hpp>
#include <shards/shards.hpp>
#include <shards/shardwrapper.hpp>
#include <shards/core/shared.hpp>
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
#include <ikcp.h>

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
    SHLOG_TRACE("NetworkContext dtor");
    boost::asio::post(_io_context, [this]() {
      // allow end/thread exit
      _io_context.stop();
    });
    _io_context_thread.join();
  }
};

struct NetworkPeer {
  NetworkPeer() {}

  ~NetworkPeer() {
    if (kcp)
      ikcp_release(kcp);
  }

  void maybeUpdate() {
    auto now = SHClock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - _start).count();
    if (ikcp_check(kcp, ms) <= ms) {
      ikcp_update(kcp, ms);
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
  }

  std::optional<udp::endpoint> endpoint{};
  ikcpcb *kcp = nullptr;
  Serialization des{};
  OwnedVar payload{};

  SHTime _start = SHClock::now();
  SHTime _lastContact = SHClock::now();

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

  ExposedInfo _required;
  SHTypeInfo compose(const SHInstanceData &data) {
    _required.clear();
    collectRequiredVariables(data.shared, _required, (SHVar &)_addr);
    collectRequiredVariables(data.shared, _required, (SHVar &)_port);
    return data.inputType;
  }

  SHExposedTypesInfo requiredVariables() { return SHExposedTypesInfo(_required); }

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
    auto &networkContext = context->anyStorage["Network.Context"];
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

  std::unordered_map<udp::endpoint, NetworkPeer *> _end2Peer;
  std::unordered_map<const SHWire *, NetworkPeer *> _wire2Peer;
  std::unique_ptr<WireDoppelgangerPool<NetworkPeer>> _pool;
  OwnedVar _handlerMaster{};

  float _timeoutSecs = 30.0f;

  static inline Parameters params{
      {"Address", SHCCSTR("The local bind address or the remote address."), {CoreInfo::StringOrStringVar}},
      {"Port", SHCCSTR("The port to bind if server or to connect to if client."), {CoreInfo::IntOrIntVar}},
      {"Handler",
       SHCCSTR("The wire to spawn for each new peer that connects, stopping that wire will break the connection."),
       {CoreInfo::WireOrNone}},
      {"Timeout",
       SHCCSTR("The timeout in seconds after which a peer will be disconnected if there is no network activity."),
       {CoreInfo::FloatType}}};

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
    default:
      return Var::Empty;
    }
  }

  SHTypeInfo compose(SHInstanceData &data) {
    if (_handlerMaster.valueType == SHType::Wire)
      _pool.reset(new WireDoppelgangerPool<NetworkPeer>(_handlerMaster.payload.wireValue));

    // inject our special context vars
    _sharedCopy = ExposedInfo(data.shared);
    auto endpointInfo = ExposedInfo::Variable("Network.Peer", SHCCSTR("The active peer."), SHTypeInfo(PeerInfo));
    _sharedCopy.push_back(endpointInfo);
    return NetworkBase::compose(data);
  }

  void gcWires() {
    if (!_stopWireQueue.empty()) {
      SHWire *toStop{};
      while (_stopWireQueue.pop(toStop)) {
        // ensure cleanup is called
        const_cast<SHWire *>(toStop)->cleanup();

        std::shared_lock<std::shared_mutex> lock(peersMutex);
        auto container = _wire2Peer[toStop];
        _pool->release(container);
        lock.unlock();

        if (_contextCopy) {
          OnPeerDisconnected event{
              .endpoint = *container->endpoint,
              .wire = container->wire,
          };
          (*_contextCopy)->main->dispatcher.trigger(std::move(event));
        }

        SHLOG_TRACE("Clearing endpoint {}", container->endpoint->address().to_string());
        std::unique_lock<std::shared_mutex> lock2(peersMutex);
        _end2Peer.erase(*container->endpoint);
      }
    }
  }

  void warmup(SHContext *context) {
    if (!_pool) {
      throw ComposeError("Peer wires pool not valid!");
    }

    _contextCopy = context;

    NetworkBase::warmup(context);
  }

  void cleanup() {
    if (_pool) {
      SHLOG_TRACE("Stopping all wires");
      _pool->stopAll();
    } else {
      SHLOG_TRACE("No pool to stop");
    }

    _contextCopy.reset();

    NetworkBase::cleanup();

    gcWires();
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
      data.shared = SHExposedTypesInfo(server._sharedCopy);
      data.wire = wire;
      wire->mesh = (*server._contextCopy)->main->mesh;
      auto res = composeWire(
          wire,
          [](const struct Shard *errorShard, SHStringWithLen errorTxt, SHBool nonfatalWarning, void *userData) {
            if (!nonfatalWarning) {
              SHLOG_ERROR(errorTxt);
              throw ActivationError("Network.Server handler wire compose failed");
            } else {
              SHLOG_WARNING(errorTxt);
            }
          },
          nullptr, data);
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
    _socket->async_receive_from(
        boost::asio::buffer(recv_buffer.data(), recv_buffer.size()), _sender,
        [this](boost::system::error_code ec, std::size_t bytes_recvd) {
          if (!ec && bytes_recvd > 0) {
            NetworkPeer *currentPeer = nullptr;
            std::shared_lock<std::shared_mutex> lock(peersMutex);
            auto it = _end2Peer.find(_sender);
            if (it == _end2Peer.end()) {
              // SHLOG_TRACE("Received packet from unknown peer: {} port: {}", _sender.address().to_string(), _sender.port());

              // new peer
              lock.unlock();

              // we write so hard lock this
              std::unique_lock<std::shared_mutex> lock(peersMutex);

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
                  peer->onStopConnection = peer->wire->dispatcher.sink<SHWire::OnStopEvent>().connect<&Server::wireOnStop>(this);
                }

                // set wire ID, in order for Events to be properly routed
                // for now we just use ptr as ID, until it causes problems
                peer->wire->id = reinterpret_cast<entt::id_type>(peer);

                currentPeer = peer;
              } catch (std::exception &e) {
                SHLOG_ERROR("Error acquiring peer: {}", e.what());

                // keep receiving
                if (_socket)
                  return do_receive();
              }
            } else {
              // SHLOG_TRACE("Received packet from known peer: {} port: {}", _sender.address().to_string(), _sender.port());

              // existing peer
              currentPeer = it->second;

              lock.unlock();
            }

            auto err = ikcp_input(currentPeer->kcp, (char *)recv_buffer.data(), bytes_recvd);
            if (err < 0) {
              SHLOG_ERROR("Error ikcp_input: {}, peer: {} port: {}", err, _sender.address().to_string(), _sender.port());
              _stopWireQueue.push(currentPeer->wire.get());
            }

            currentPeer->_lastContact = SHClock::now();

            // keep receiving
            if (_socket)
              return do_receive();
          } else {
            SHLOG_DEBUG("Error receiving: {}, peer: {} port: {}", ec.message(), _sender.address().to_string(), _sender.port());

            std::shared_lock<std::shared_mutex> lock(peersMutex);
            auto it = _end2Peer.find(_sender);
            if (it != _end2Peer.end()) {
              SHLOG_TRACE("Removing peer: {} port: {}", _sender.address().to_string(), _sender.port());
              _stopWireQueue.push(it->second->wire.get());
            }

            // keep receiving
            if (_socket)
              return do_receive();
          }
        });
  }

  std::vector<uint8_t> _buffer;

  ExposedInfo _sharedCopy;
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

    gcWires();

    {
      auto now = SHClock::now();

      std::shared_lock<std::shared_mutex> lock(peersMutex);

      for (auto &[end, peer] : _end2Peer) {
        if (now > (peer->_lastContact + SHDuration(_timeoutSecs))) {
          SHLOG_DEBUG("Peer {} timed out", end.address().to_string());
          _stopWireQueue.push(peer->wire.get());
          continue;
        }

        peer->maybeUpdate();

        setPeer(context, *peer);

        if (!peer->wire->warmedUp) {
          peer->wire->warmup(context);

          OnPeerConnected event{
              .endpoint = *peer->endpoint,
              .wire = peer->wire,
          };
          context->main->dispatcher.trigger(std::move(event));
        }

        auto nextSize = ikcp_peeksize(peer->kcp);
        while (nextSize > 0) {
          _buffer.resize(nextSize);
          auto size = ikcp_recv(peer->kcp, (char *)_buffer.data(), nextSize);
          assert(size == nextSize);

          // deserialize from buffer on top of the vector of payloads, wires might consume them out of band
          Reader r((char *)_buffer.data(), nextSize);
          peer->des.reset();
          peer->des.deserialize(r, peer->payload);

          // Run within the root flow
          auto runRes = runSubWire(peer->wire.get(), context, peer->payload);
          if (unlikely(runRes.state == SHRunWireOutputState::Failed) || unlikely(runRes.state == SHRunWireOutputState::Stopped)) {
            stop(peer->wire.get());
            // Always continue, on stop event will cleanup
            context->continueFlow();
            nextSize = 0; // exit this peer
          } else {
            nextSize = ikcp_peeksize(peer->kcp);
          }
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
    _exposing[0].help = SHCCSTR("The required peer.");
    _exposing[0].exposedType = Client::PeerType;
    return {_exposing.data(), 1, 0};
  }

  static inline Type PeerType{{SHType::Object, {.object = {.vendorId = CoreCC, .typeId = PeerCC}}}};
  static inline Type PeerObjectType = Type::VariableOf(PeerType);

  static SHTypesInfo outputTypes() { return PeerType; }

  NetworkPeer _peer;
  ShardsVar _blks{};

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
    _peer.reset();
    _peer.kcp->user = this;
    _peer.kcp->output = &Client::udp_output;
  }

  void do_receive() {
    thread_local std::vector<uint8_t> recv_buffer(0xFFFF);
    _socket->async_receive_from(boost::asio::buffer(recv_buffer.data(), recv_buffer.size()), _server,
                                [this](boost::system::error_code ec, std::size_t bytes_recvd) {
                                  if (!ec && bytes_recvd > 0) {
                                    auto err = ikcp_input(_peer.kcp, (char *)recv_buffer.data(), bytes_recvd);
                                    if (err < 0) {
                                      SHLOG_ERROR("Error ikcp_input: {}");
                                    }

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
    return NetworkBase::compose(data);
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
      udp::resolver::query query(udp::v4(), SHSTRING_PREFER_SHSTRVIEW(_addr.get()), sport);
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

    return Var::Object(&_peer, CoreCC, PeerCC);
  }

  udp::endpoint _server;
};

struct PeerBase {
  std::array<SHExposedTypeInfo, 1> _requiring;
  SHVar *_peerVar = nullptr;
  ParamVar _peerParam;

  void cleanup() {
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
    if (fixedPeer.valueType == SHType::Object) {
      assert(fixedPeer.payload.objectVendorId == CoreCC);
      assert(fixedPeer.payload.objectTypeId == PeerCC);
      return reinterpret_cast<NetworkPeer *>(fixedPeer.payload.objectValue);
    } else {
      if (!_peerVar) {
        _peerVar = referenceVariable(context, "Network.Peer");
      }
      assert(_peerVar->payload.objectVendorId == CoreCC);
      assert(_peerVar->payload.objectTypeId == PeerCC);
      return reinterpret_cast<NetworkPeer *>(_peerVar->payload.objectValue);
    }
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
    // TODO add _peerParam
    _requiring[0].name = "Network.Peer";
    _requiring[0].help = SHCCSTR("The required peer.");
    _requiring[0].exposedType = Client::PeerType;
    return {_requiring.data(), 1, 0};
  }
};

struct Send : public PeerBase {
  // Must take an optional seq of SocketData, to be used properly by server
  // This way we get also a easy and nice broadcast

  ThreadShared<std::array<char, 0xFFFF>> _send_buffer;

  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return CoreInfo::AnyType; }

  Serialization serializer;

  SHVar activate(SHContext *context, const SHVar &input) {
    auto peer = getPeer(context);
    NetworkBase::Writer w(&_send_buffer().front(), _send_buffer().size());
    serializer.reset();
    auto size = serializer.serialize(input, w);
    auto err = ikcp_send(peer->kcp, (char *)_send_buffer().data(), size);
    if (err < 0) {
      SHLOG_ERROR("ikcp_send error: {}", err);
      throw ActivationError("ikcp_send error");
    }
    return input;
  }
};

struct PeerID : public PeerBase {
  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return CoreInfo::IntType; }

  SHVar activate(SHContext *context, const SHVar &input) {
    auto peer = getPeer(context);
    return Var(reinterpret_cast<entt::id_type>(peer));
  }
};
}; // namespace Network

}; // namespace shards
SHARDS_REGISTER_FN(network) {
  using namespace shards::Network;
  REGISTER_SHARD("Network.Server", Server);
  REGISTER_SHARD("Network.Client", Client);
  REGISTER_SHARD("Network.Send", Send);
  REGISTER_SHARD("Network.PeerID", PeerID);
}
