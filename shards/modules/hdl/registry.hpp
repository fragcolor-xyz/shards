/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2024 Fragcolor Pte. Ltd. */

#ifndef SHARDS_MODULE_HDL_REGISTRY_HPP
#define SHARDS_MODULE_HDL_REGISTRY_HPP

#include <shards/shards.hpp>
#include <string>
#include <map>
#include <functional>

namespace shards {
namespace hdl {

// Forward declaration
struct HDLContext;

// Handler interface for translating shards to HDL
struct IHDLHandler {
  virtual ~IHDLHandler() = default;
  virtual void translate(Shard *shard, HDLContext &context) = 0;
};

// Registry that maps shard names to HDL translation handlers
struct HDLRegistry {
private:
  std::map<std::string, IHDLHandler *> _handlers;

public:
  // Register a handler for a shard name
  void registerHandler(const char *shardName, IHDLHandler *handler);

  // Look up a handler for a shard
  IHDLHandler *resolve(Shard *shard);

  // Check if a handler exists for a shard
  bool hasHandler(Shard *shard) const;
};

// Get the global HDL registry
HDLRegistry &getHDLRegistry();

// Helper macro for registering HDL handlers
#define REGISTER_HDL_HANDLER(shardName, HandlerClass)                                                                            \
  namespace {                                                                                                                    \
  struct HandlerClass##Registrar {                                                                                               \
    static HandlerClass handler;                                                                                                 \
    HandlerClass##Registrar() { shards::hdl::getHDLRegistry().registerHandler(shardName, &handler); }                            \
  } handlerClass##Instance;                                                                                                      \
  HandlerClass HandlerClass##Registrar::handler;                                                                                 \
  }

// Template for creating simple handlers from functions
template <typename F> struct FunctionHandler : public IHDLHandler {
  F func;
  FunctionHandler(F f) : func(f) {}
  void translate(Shard *shard, HDLContext &context) override { func(shard, context); }
};

// Helper to create function-based handlers
template <typename F> std::unique_ptr<IHDLHandler> makeHandler(F func) {
  return std::make_unique<FunctionHandler<F>>(func);
}

} // namespace hdl
} // namespace shards

#endif // SHARDS_MODULE_HDL_REGISTRY_HPP
