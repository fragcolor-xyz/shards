/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2026 Fragcolor Pte. Ltd. */

#include <shards/core/shared.hpp>
#include <shards/core/runtime.hpp>

namespace shards {
namespace example {

struct Square {
  static SHOptionalString help() { return SHCCSTR("This shard squares the input integer."); }
  static SHOptionalString inputHelp() { return SHCCSTR("The integer to square."); }
  static SHOptionalString outputHelp() { return SHCCSTR("The squared integer."); }

  static SHTypesInfo inputTypes() { return CoreInfo::IntType; }
  static SHTypesInfo outputTypes() { return CoreInfo::IntType; }

  SHVar activate(SHContext *context, const SHVar &input) { return Var(input.payload.intValue * input.payload.intValue); }
};

} // namespace example

// Expands to shardsRegister_example_cpp, referenced by the generated registry
SHARDS_REGISTER_FN(cpp) { REGISTER_SHARD("Example.Square", example::Square); }
} // namespace shards
