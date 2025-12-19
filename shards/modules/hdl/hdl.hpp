/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2024 Fragcolor Pte. Ltd. */

#ifndef SHARDS_MODULE_HDL_HPP
#define SHARDS_MODULE_HDL_HPP

#include "types.hpp"
#include "blocks.hpp"
#include "context.hpp"
#include "registry.hpp"
#include "verilog_emitter.hpp"

namespace shards {
namespace hdl {

// Register all HDL translation handlers
void registerAllHDLShards();

} // namespace hdl
} // namespace shards

#endif // SHARDS_MODULE_HDL_HPP
