/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

#ifndef SANDBOX_H
#define SANDBOX_H

#include "shared.hpp"

namespace shards {
namespace Sandbox {

struct Sand {

  static SHTypesInfo inputTypes() { return CoreInfo::NoneType; }
  static SHTypesInfo outputTypes() { return CoreInfo::AnyType; }

  static inline Parameters _params{};
  static SHParametersInfo parameters() { return _params; }

  SHVar activate(SHContext *context, const SHVar &input) {
    SHLOG_TRACE("Sand activate()-d");
    return Var();
  }
};

void registerShards() {
  SHLOG_TRACE("Registering Sand");
  REGISTER_SHARD("Sandbox.Sand", Sand);
}
} // namespace Sandbox
} // namespace shards

#endif /* SANDBOX_H */
