/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2019 Fragcolor Pte. Ltd. */

#include <shards/core/module.hpp>
#include "flow.hpp"

namespace shards {
SHARDS_REGISTER_FN(flow) {
  REGISTER_SHARD("Cond", Cond);
  REGISTER_SHARD("Maybe", Maybe);
  REGISTER_SHARD("Await", Await);
  REGISTER_SHARD("When", When<true>);
  REGISTER_SHARD("WhenNot", When<false>);
  REGISTER_SHARD("If", IfBlock);
  REGISTER_SHARD("Match", Match);
  REGISTER_SHARD("SubFlow", Sub);
}
}; // namespace shards
