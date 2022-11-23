/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2019 Fragcolor Pte. Ltd. */

#ifndef SH_CORE_SHARDS_LOGGING
#define SH_CORE_SHARDS_LOGGING

#include "shared.hpp"

namespace shards {
struct Enums {
  enum class LogLevel { Trace, Debug, Info, Warning, Error };
  DECL_ENUM_INFO(LogLevel, LogLevel, 'logL');
};
}; // namespace shards

#endif // SH_CORE_SHARDS_LOGGING
