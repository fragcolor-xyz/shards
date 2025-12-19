/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2024 Fragcolor Pte. Ltd. */

#include "registry.hpp"

namespace shards {
namespace hdl {

HDLRegistry &getHDLRegistry() {
  static HDLRegistry instance;
  return instance;
}

void HDLRegistry::registerHandler(const char *shardName, IHDLHandler *handler) {
  _handlers[shardName] = handler;
}

IHDLHandler *HDLRegistry::resolve(Shard *shard) {
  if (shard == nullptr) {
    return nullptr;
  }
  auto it = _handlers.find(shard->name(shard));
  if (it != _handlers.end()) {
    return it->second;
  }
  return nullptr;
}

bool HDLRegistry::hasHandler(Shard *shard) const {
  if (shard == nullptr) {
    return false;
  }
  return _handlers.count(shard->name(shard)) > 0;
}

// Forward declarations for registration functions
void registerHDLCoreShards();
void registerHDLIOShards();
void registerHDLMathShards();

void registerAllHDLShards() {
  registerHDLCoreShards();
  registerHDLIOShards();
  registerHDLMathShards();
}

} // namespace hdl
} // namespace shards
