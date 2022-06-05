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

  static inline Parameters _params{{"grains", SHCCSTR("of sand."), {CoreInfo::IntType}}};
  static SHParametersInfo parameters() { return _params; }

  SHVar getParam(int index) {
    if (index == 0)
      return Var(int64_t(_grains));

    return Var();
  }

  void setParam(int index, const SHVar &value) {
    if (index == 0)
      _grains = uint32_t(value.payload.intValue);
  }

  void warmup(SHContext *context) {
    SHLOG_TRACE("Getting the sand wet");
  }

  void cleanup() {
    _grains = 0;
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    SHLOG_TRACE("Sand activate()-d");
    return Var();
  }

private:
  uint32_t _grains{1};
};

void registerShards() {
  SHLOG_TRACE("Registering Sand");
  REGISTER_SHARD("Sandbox.Sand", Sand);
}
} // namespace Sandbox
} // namespace shards

#endif /* SANDBOX_H */
