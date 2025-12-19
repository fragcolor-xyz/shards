// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2020-2024 Fragcolor Pte. Ltd.

// Minimal logging stub for freestanding/bare-metal targets
// This provides empty implementations of the logging API

// Define SHARDS_NO_SPDLOG before including log.hpp
#ifndef SHARDS_NO_SPDLOG
#define SHARDS_NO_SPDLOG 1
#endif

#include "log.hpp"

namespace shards::logging {

// Empty implementation - logging is disabled on freestanding
void init(Options options) {
  // No-op
}

void shutdown() {
  // No-op
}

Logger getOrCreate(std::string_view name, Level level) {
  return nullptr;  // Return null logger
}

Level parseLevel(std::string_view str) {
  return Level::off;
}

// ThreadState stub
namespace {
static ThreadState g_state;
}

ThreadState &ThreadState::get() {
  return g_state;
}

} // namespace shards::logging
