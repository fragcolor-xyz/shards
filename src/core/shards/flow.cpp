/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2019 Fragcolor Pte. Ltd. */

#include "flow.hpp"
#include "foundation.hpp"
#include "runtime.hpp"

namespace shards {

struct MainCall {
  void action() {
    void *stack = nullptr;
    void *stackPtr = &stack;
    SHLOG_INFO("Hello from a stack {}", stackPtr);
  }
};

SHVar testMainTask(SHContext *context, const SHVar &input) {
  MainCall c;
  callOnMainThread(context, c); // this will call c.action() on the main thread, use this to call SDL shit on android
  return input;
}

SHVar testCoroTask(SHContext *context, const SHVar &input) {
  MainCall c;
  c.action();
  return input;
}

void registerFlowShards() {
  using TestMainShard = LambdaShard<testMainTask, CoreInfo::AnyType, CoreInfo::AnyType>;
  REGISTER_SHARD("TestMainShard", TestMainShard);
  using TestCoroShard = LambdaShard<testCoroTask, CoreInfo::AnyType, CoreInfo::AnyType>;
  REGISTER_SHARD("TestCoroShard", TestCoroShard);
  REGISTER_SHARD("Cond", Cond);
  REGISTER_SHARD("Maybe", Maybe);
  REGISTER_SHARD("Await", Await);
  REGISTER_SHARD("When", When<true>);
  REGISTER_SHARD("WhenNot", When<false>);
  REGISTER_SHARD("If", IfBlock);
  REGISTER_SHARD("Match", Match);
  REGISTER_SHARD("Sub", Sub);
  REGISTER_SHARD("Hashed", HashedShards);
  REGISTER_SHARD("Worker", Worker);
}
}; // namespace shards
