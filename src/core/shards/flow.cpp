/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2019 Fragcolor Pte. Ltd. */

#include "flow.hpp"

namespace shards {
void registerFlowShards() {
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
