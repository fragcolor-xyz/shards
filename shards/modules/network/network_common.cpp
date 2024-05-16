/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2019 Fragcolor Pte. Ltd. */

#include "network.hpp"

#include <shards/core/shared.hpp>
#include <shards/core/params.hpp>
#include <shards/common_types.hpp>
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

namespace shards {
namespace Network {

Writer &getSendWriter() {
  static thread_local Writer inst;
  return inst;
}

void Reader::deserializeInto(OwnedVar &output) {
  static thread_local Serialization serializer;
  serializer.reset();
  serializer.deserialize(*this, output);
}

boost::span<const uint8_t> Writer::varToSendBuffer(const SHVar &input) {
  static thread_local Serialization serializer;

  reset();
  serializer.reset();
  serializer.serialize(input, *this);
  finalize();

  return boost::span(data(), size());
}

Peer &getConnectedPeer(ParamVar &peerParam) {
  Peer &peer = varAsObjectChecked<Peer>(peerParam.get(), Types::Peer);
  if (peer.disconnected()) {
    throw ActivationError("Peer is disconnected");
  }
  return peer;
}

void setDefaultPeerParam(ParamVar &peerParam) {
  peerParam = Var("Network.Peer");
  peerParam->valueType = SHType::ContextVar;
}

Peer &getServer(ParamVar &serverParam) {
  Peer &peer = varAsObjectChecked<Peer>(serverParam.get(), Types::Server);
  return peer;
}

void setDefaultSetverParam(ParamVar &peerParam) {
  peerParam = Var("Network.Server");
  peerParam->valueType = SHType::ContextVar;
}

struct Broadcast {
  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return CoreInfo::AnyType; }

  PARAM_PARAMVAR(_server, "Server", "The server", {Types::Server});
  PARAM_IMPL(PARAM_IMPL_FOR(_server));

  Broadcast() {
    _server = Var("Network.Server");
    _server->valueType = SHType::ContextVar;
  }

  void warmup(SHContext *context) { PARAM_WARMUP(context); }
  void cleanup(SHContext *context) { PARAM_CLEANUP(context); }

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    return outputTypes().elements[0];
  }

  Server &getServer() { return varAsObjectChecked<Server>(_server.get(), Types::Server); }

  SHVar activate(SHContext *context, const SHVar &input) {
    auto &server = getServer();
    server.broadcastVar(input);
    return input;
  }
};

struct Send {
  static SHTypesInfo inputTypes() { return shards::CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return shards::CoreInfo::AnyType; }
  static SHOptionalString help() { return SHCCSTR(""); }

  PARAM_EXT(ParamVar, _peer, Types::PeerParameterInfo);
  PARAM_IMPL(PARAM_IMPL_FOR(_peer));

  Send() { setDefaultPeerParam(_peer); }

  void warmup(SHContext *context) { PARAM_WARMUP(context); }
  void cleanup(SHContext *context) { PARAM_CLEANUP(context); }

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    return outputTypes().elements[0];
  }

  SHVar activate(SHContext *shContext, const SHVar &input) {
    auto &peer = getConnectedPeer(_peer);
    peer.sendVar(input);
    return input;
  }
};

struct PeerID {
  static SHTypesInfo inputTypes() { return shards::CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return shards::CoreInfo::IntType; }
  static SHOptionalString help() { return SHCCSTR(""); }

  PARAM_EXT(ParamVar, _peer, Types::PeerParameterInfo);
  PARAM_IMPL(PARAM_IMPL_FOR(_peer));

  PeerID() { setDefaultPeerParam(_peer); }

  void warmup(SHContext *context) { PARAM_WARMUP(context); }
  void cleanup(SHContext *context) { PARAM_CLEANUP(context); }

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    return outputTypes().elements[0];
  }

  SHVar activate(SHContext *shContext, const SHVar &input) {
    auto &peer = getConnectedPeer(_peer);
    return Var(peer.getId());
  }
};
}; // namespace Network
}; // namespace shards

SHARDS_REGISTER_FN(network_common) {
  using namespace shards::Network;
  REGISTER_SHARD("Network.Broadcast", Broadcast);
  REGISTER_SHARD("Network.Send", Send);
  REGISTER_SHARD("Network.PeerID", PeerID);
}