#include "network.hpp"
#include <shards/core/shared.hpp>
#include <shards/core/params.hpp>
#include <shards/common_types.hpp>
#include <shards/core/wire_doppelganger_pool.hpp>
#include <shards/core/into_wire.hpp>
#include <shards/core/object_var_util.hpp>
#include <boost/lockfree/queue.hpp>

extern "C" {
#include "pollnet.h"
}

namespace shards {
namespace Network {

std::string_view pollnet_unsafe_get_data_string_view(pollnet_ctx *ctx, sockethandle_t handle) {
  size_t len = pollnet_get_data_size(ctx, handle);
  return std::string_view((const char *)pollnet_unsafe_get_data_ptr(ctx, handle), len);
}
boost::span<const uint8_t> pollnet_unsafe_get_data_span(pollnet_ctx *ctx, sockethandle_t handle) {
  size_t len = pollnet_get_data_size(ctx, handle);
  return boost::span((const uint8_t *)pollnet_unsafe_get_data_ptr(ctx, handle), len);
}

struct WSServer final : public Server {
  pollnet_ctx *ctx{};
  sockethandle_t socket{};
  std::unordered_map<sockethandle_t, struct WSHandler *> handle2Peer;

  WSServer() { ctx = pollnet_init(); }
  ~WSServer() { pollnet_shutdown(ctx); }

  void bind(std::string_view addr, int port) {
    std::string fullAddr = fmt::format("{}:{}", addr, port);
    socket = pollnet_listen_ws(ctx, toSWL(fullAddr));
  }

  void close() {
    shassert(handle2Peer.empty());
    pollnet_close(ctx, socket);
    socket = pollnet_invalid_handle();
  }

  void broadcast(boost::span<const uint8_t> data);
};

struct WSPeer : public Peer {
  pollnet_ctx *ctx{};
  sockethandle_t socket{};
  std::string debugName;
  bool disconnected_{};

  void init(pollnet_ctx *ctx_, sockethandle_t handle, bool isServerSide) {
    ctx = ctx_;
    socket = handle;
    debugName = isServerSide ? fmt::format("WSServerPeer({})", socket) : fmt::format("WSClientPeer({})", socket);
  }

  void close() {
    pollnet_close(ctx, socket);
    disconnected_ = true;
    socket = pollnet_invalid_handle();
  }

  void send(boost::span<const uint8_t> data) { pollnet_send_binary(ctx, socket, data.data(), data.size()); }
  bool disconnected() const { return disconnected_; }
  int64_t getId() const { return int64_t(socket); }
  std::string_view getDebugName() const { return debugName; }
};
struct WSHandler : public WSPeer {
  entt::connection onStopConnection;
  std::shared_ptr<SHWire> wire;
  OwnedVar recvBuffer;
};

void WSServer::broadcast(boost::span<const uint8_t> data) {
  for (auto &[handle, peer] : handle2Peer) {
    peer->send(data);
  }
}

inline void pollnetLog(pollnet_ctx *ctx, socketstatus_t status, sockethandle_t handle, std::string_view head) {
  switch (status) {
  case POLLNET_INVALID:
    SPDLOG_LOGGER_ERROR(getLogger(), "{}> POLLNET_INVALID", head);
    break;
  case POLLNET_OPENING:
    SPDLOG_LOGGER_TRACE(getLogger(), "{}> POLLNET_OPENING", head);
    break;
  case POLLNET_CLOSED:
    SPDLOG_LOGGER_DEBUG(getLogger(), "{}> POLLNET_CLOSED, {}", head, handle);
    break;
  case POLLNET_OPEN_NODATA:
    break;
  case POLLNET_OPEN_HASDATA: {
    SPDLOG_LOGGER_TRACE(getLogger(), "{}> POLLNET_OPEN_HASDATA, {} bytes", head, pollnet_get_data_size(ctx, handle));
  } break;
  case POLLNET_ERROR: {
    SPDLOG_LOGGER_ERROR(getLogger(), "{}> POLLNET_ERROR, {}", head, pollnet_unsafe_get_data_string_view(ctx, handle));
  } break;
  case POLLNET_OPEN_NEWCLIENT: {
    auto clientHandle = pollnet_get_connected_client_handle(ctx, handle);
    SPDLOG_LOGGER_DEBUG(getLogger(), "{}> POLLNET_OPEN_NEWCLIENT, {}", head, clientHandle);
  } break;
  }
}

struct WSServerShard {
  static SHTypesInfo inputTypes() { return shards::CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return Types::Server; }
  static SHOptionalString help() { return SHCCSTR(""); }

  PARAM_PARAMVAR(_address, "Address", ("The local bind address or the remote address."), {CoreInfo::StringOrStringVar});
  PARAM_PARAMVAR(_port, "Port", ("The port to bind if server or to connect to if client."), {CoreInfo::IntOrIntVar});
  PARAM_VAR(_handler, "Handler",
            ("The wire to spawn for each new peer that connects, stopping that wire will break the connection."),
            {IntoWire::RunnableTypes});
  PARAM_VAR(_timeout, "Timeout",
            ("The timeout in seconds after which a peer will be disconnected if there is no network activity."),
            {CoreInfo::FloatType});
  PARAM(ShardsVar, _onDisconnect, "OnDisconnect", ("The flow to execute when a peer disconnects, The Peer ID will be the input."),
        {CoreInfo::ShardsOrNone});
  PARAM_IMPL(PARAM_IMPL_FOR(_address), PARAM_IMPL_FOR(_port), PARAM_IMPL_FOR(_handler), PARAM_IMPL_FOR(_timeout),
             PARAM_IMPL_FOR(_onDisconnect));

  std::unique_ptr<WireDoppelgangerPool<WSHandler>> _pool;
  ExposedInfo _sharedCopy;
  std::shared_ptr<SHMesh> _mesh;
  std::shared_ptr<WSServer> _server;
  SHVar _serverVar;
  SHVar *_serverVarRef{};
  SHVar *_peerVarRef{};

  std::array<SHExposedTypeInfo, 1> _exposing;

  boost::lockfree::queue<const SHWire *> _stopWireQueue{16};

  struct Composer {
    WSServerShard &server;
    
    Composer(WSServerShard &server) : server(server) {}
    void compose(SHWire *wire, void *nothing, bool recycling) {
      if (recycling)
        return;

      SHInstanceData data{};
      data.inputType = CoreInfo::AnyType;
      data.shared = SHExposedTypesInfo(server._sharedCopy);
      data.wire = wire;
      wire->mesh = server._mesh;
      auto res = composeWire(wire, data);
      arrayFree(res.exposedInfo);
      arrayFree(res.requiredInfo);
    }
  } _composer;

  WSServerShard() : _composer(*this) {}

  std::string _serverDebugName;
  void warmup(SHContext *context) {
    PARAM_WARMUP(context);
    _mesh = context->main->mesh.lock();
    _serverVarRef = referenceVariable(context, "Network.Server");
    _peerVarRef = referenceVariable(context, "Network.Peer");
  }

  void cleanup(SHContext *context) {
    PARAM_CLEANUP(context);

    if (_pool) {
      SPDLOG_LOGGER_TRACE(getLogger(), "Stopping all wires");
      _pool->stopAll();
    }

    if (_server) {
      for (auto &[handle, peer] : _server->handle2Peer) {
        clientDisconnectedNoUnmap(*peer, nullptr);
      }
      _server->handle2Peer.clear();
      _server->close();
      _server.reset();
    }

    if (_serverVarRef) {
      assignVariableValue(*_serverVarRef, Var::Empty);
      releaseVariable(_serverVarRef);
      _serverVarRef = nullptr;
    }
    if (_peerVarRef) {
      assignVariableValue(*_peerVarRef, Var::Empty);
      releaseVariable(_peerVarRef);
      _peerVarRef = nullptr;
    }
    _mesh.reset();
  }

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);

    if (!_handler->isNone()) {
      std::shared_ptr<SHWire> wire = IntoWire{}.defaultWireName("network-wire").var(_handler);
      _pool.reset(new WireDoppelgangerPool<WSHandler>(SHWire::weakRef(wire)));
    }

    // compose first with data the _disconnectionHandler if any
    if (_onDisconnect) {
      SHInstanceData dataCopy = data;
      dataCopy.inputType = Types::Peer;
      _onDisconnect.compose(dataCopy);
    }

    // inject our special context vars
    _sharedCopy = ExposedInfo(data.shared);
    auto endpointInfo = ExposedInfo::Variable("Network.Peer", SHCCSTR("The active peer."), Types::Peer);
    _sharedCopy.push_back(endpointInfo);

    return outputTypes().elements[0];
  }

  SHExposedTypesInfo exposedVariables() {
    _exposing[0].name = "Network.Server";
    _exposing[0].help = SHCCSTR("The exposed server.");
    _exposing[0].exposedType = Types::Server;
    _exposing[0].isProtected = true;
    return {_exposing.data(), 1, 0};
  }

  void wireStopped(const SHWire::OnStopEvent &e) { _stopWireQueue.push(e.wire); }

  void newClient(WSServer &server, SHContext *context) {
    auto peer = _pool->acquire(_composer);
    peer->init(server.ctx, pollnet_get_connected_client_handle(server.ctx, server.socket), true);
    server.handle2Peer[peer->socket] = peer;

    if (!peer->onStopConnection) {
      peer->onStopConnection = _mesh->dispatcher.sink<SHWire::OnStopEvent>().connect<&WSServerShard::wireStopped>(this);
    }

    OnPeerConnected event{
        // .endpoint = *peer->endpoint,
        .wire = peer->wire,
    };
    _mesh->dispatcher.trigger(std::move(event));
    peer->wire->warmup(context);
  }

  void clientDisconnectedNoUnmap(WSHandler &handler, SHContext *context) {
    if (_onDisconnect && context) {
      auto input = Var::Object(&(Peer &)handler, Types::Peer);
      SHVar output{};
      _onDisconnect.activate(context, input, output);
    }

    auto toStop = handler.wire;
    auto wireState = toStop->state.load();
    if (wireState != SHWire::State::Failed && wireState != SHWire::State::Stopped) {
      toStop->cleanup();
    } else {
      // in the above cases cleanup already happened
      SPDLOG_LOGGER_TRACE(getLogger(), "Wire {} already stopped, state: {}", toStop->name, wireState);
    }

    if (context) {
      // now fire events
      OnPeerDisconnected event{
          // .endpoint = *container->endpoint,
          .wire = handler.wire,
      };
      _mesh->dispatcher.trigger(std::move(event));
    }

    handler.close();

    _pool->release(&handler);
  }

  void recvClientData(WSHandler &handler, SHContext *context) {
    withObjectVariable(*_peerVarRef, &handler, Types::Peer, [&]() {
      auto dataSpan = pollnet_unsafe_get_data_span(handler.ctx, handler.socket);
      // Var tmp(dataSpan.data(), dataSpan.size());
      // cloneVar(handler.recvBuffer, tmp);

      try {
        // deserialize from buffer on top of the vector of payloads, wires might consume them out of band
        Reader r((char *)dataSpan.data() + 4, dataSpan.size() - 4);
        r.deserializeInto(handler.recvBuffer);

        auto runRes = shards::runSubWire(handler.wire.get(), context, handler.recvBuffer);
        if (unlikely(runRes.state == SHRunWireOutputState::Failed || runRes.state == SHRunWireOutputState::Stopped ||
                     runRes.state == SHRunWireOutputState::Returned)) {
          handler.disconnected_ = true;
          shards::stop(handler.wire.get());
        }
      } catch (std::exception &e) {
        SPDLOG_LOGGER_ERROR(getLogger(), "Error while processing data from peer {}: {}", handler.getDebugName(), e.what());
        shards::stop(handler.wire.get());
      }

      // Always adjust the context back to continue, peer wire might have changed it
      context->resetErrorStack();
      context->continueFlow();
    });
  }

  SHVar activate(SHContext *shContext, const SHVar &input) {
    if (!_server) {
      _server = std::make_shared<WSServer>();
      _server->bind(SHSTRVIEW(_address.get()), _port.get().payload.intValue);
      _serverDebugName = fmt::format("WSServer({})", _server->socket);
      _serverVar = Var::Object(_server.get(), Types::Server);
      assignVariableValue(*_serverVarRef, _serverVar);
    }

    socketstatus_t status = pollnet_update(_server->ctx, _server->socket);
    pollnetLog(_server->ctx, status, _server->socket, _serverDebugName);

    switch (status) {
    case POLLNET_ERROR:
      throw ActivationError("Error in socket");
      break;
    case POLLNET_INVALID:
      throw ActivationError("Invalid socket");
      break;
    case POLLNET_CLOSED:
      throw ActivationError("Closed socket");
    case POLLNET_OPEN_NEWCLIENT:
      newClient(*_server.get(), shContext);
      break;
    }

    auto &map = _server->handle2Peer;
    for (auto it = map.begin(); it != map.end();) {
      auto handle = it->first;
      auto peer = it->second;

      socketstatus_t status = pollnet_update(_server->ctx, handle);
      pollnetLog(_server->ctx, status, handle, peer->getDebugName());
      switch (status) {
      case POLLNET_ERROR:
      case POLLNET_INVALID:
      case POLLNET_CLOSED:
        peer->disconnected_ = true;
        clientDisconnectedNoUnmap(*peer, shContext);
        _server->handle2Peer.erase(handle);
        break;
      case POLLNET_OPEN_HASDATA: {
        recvClientData(*peer, shContext);
      } break;
      }

      if (peer->disconnected_) {
        clientDisconnectedNoUnmap(*peer, shContext);
        _pool->release(peer);
        it = map.erase(it);
      } else {
        ++it;
      }
    }

    return _serverVar;
  }
};

struct WSClient {
  pollnet_ctx *ctx{};
  WSPeer peer;
  OwnedVar recvBuffer;

  WSClient() { ctx = pollnet_init(); }
  ~WSClient() { pollnet_shutdown(ctx); }
};

struct WSClientShard {
  static SHTypesInfo inputTypes() { return shards::CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return Types::Peer; }
  static SHOptionalString help() { return SHCCSTR(""); }

  PARAM_PARAMVAR(_address, "Address", ("The local bind address or the remote address."), {CoreInfo::StringOrStringVar});
  PARAM_PARAMVAR(_port, "Port", ("The port to bind if server or to connect to if client."), {CoreInfo::IntOrIntVar});
  PARAM(ShardsVar, _handler, "Handler", ("The flow to execute when a packet is received."), {CoreInfo::ShardsOrNone});
  PARAM_IMPL(PARAM_IMPL_FOR(_address), PARAM_IMPL_FOR(_port), PARAM_IMPL_FOR(_handler));

  std::shared_ptr<WSClient> _client;
  SHVar _peerVar;
  SHVar *_peerVarRef{};

  std::array<SHExposedTypeInfo, 1> _exposing;

  void warmup(SHContext *context) {
    PARAM_WARMUP(context);
    _peerVarRef = referenceVariable(context, "Network.Peer");
  }

  void cleanup(SHContext *context) {
    PARAM_CLEANUP(context);

    if (_client) {
      _client->peer.close();
      _client.reset();
    }

    if (_peerVarRef) {
      releaseVariable(_peerVarRef);
      _peerVarRef = nullptr;
    }
  }

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);

    if (_handler) {
      SHInstanceData dataCopy = data;
      ExposedInfo sharedCopy(data.shared);
      sharedCopy.push_back(ExposedInfo::Variable("Network.Peer", SHCCSTR("The active peer."), Types::Peer));
      dataCopy.shared = (SHExposedTypesInfo)sharedCopy;
      dataCopy.inputType = CoreInfo::AnyType;
      _handler.compose(dataCopy);
    }

    return outputTypes().elements[0];
  }

  SHExposedTypesInfo exposedVariables() {
    _exposing[0].name = "Network.Peer";
    _exposing[0].help = SHCCSTR("The exposed peer.");
    _exposing[0].exposedType = Types::Peer;
    _exposing[0].isProtected = true;
    return {_exposing.data(), 1, 0};
  }

  void recvClientData(WSClient &client, SHContext *context) {
    auto &peer = client.peer;
    auto dataSpan = pollnet_unsafe_get_data_span(peer.ctx, peer.socket);
    Var tmp(dataSpan.data(), dataSpan.size());

    withObjectVariable(*_peerVarRef, &peer, Types::Peer, [&]() {
      Reader r((char *)dataSpan.data() + 4, dataSpan.size() - 4);
      r.deserializeInto(client.recvBuffer);

      SHVar output{};
      if (_handler) {
        _handler.activate(context, client.recvBuffer, output);
      }
    });
  }

  SHVar activate(SHContext *shContext, const SHVar &input) {
    if (!_client) {
      _client = std::make_shared<WSClient>();
      _client->peer.init(_client->ctx,
                         pollnet_open_ws(_client->ctx, toSWL(fmt::format("{}:{}", _address.get(), _port.get().payload.intValue))),
                         false);
      _peerVar = Var::Object(&_client->peer, Types::Peer);
      assignVariableValue(*_peerVarRef, _peerVar);
    }

    auto &peer = _client->peer;
    socketstatus_t status = pollnet_update(peer.ctx, peer.socket);
    pollnetLog(peer.ctx, status, peer.socket, peer.debugName);

    switch (status) {
    case POLLNET_ERROR:
      throw ActivationError("Error in socket");
      break;
    case POLLNET_INVALID:
      throw ActivationError("Invalid socket");
      break;
    case POLLNET_CLOSED:
      throw ActivationError("Socket closed");
    case POLLNET_OPEN_HASDATA: {
      recvClientData(*_client.get(), shContext);
    } break;
    }

    return _peerVar;
  }
};

} // namespace Network
} // namespace shards

SHARDS_REGISTER_FN(network_ws) {
  using namespace shards::Network;
  REGISTER_SHARD("Network.WS.Server", WSServerShard);
  REGISTER_SHARD("Network.WS.Client", WSClientShard);
  // REGISTER_SHARD("Network.Broadcast", Broadcast);
  // REGISTER_SHARD("Network.Send", Send);
  // REGISTER_SHARD("Network.PeerID", PeerID);
  // REGISTER_SHARD("Network.Peer", GetPeer);
}
