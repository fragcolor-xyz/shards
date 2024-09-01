/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2019 Fragcolor Pte. Ltd. */

#ifndef SH_CORE_SHARDS_LOGGING
#define SH_CORE_SHARDS_LOGGING

#include <shards/core/shared.hpp>

namespace shards {
struct Enums {
  enum class LogLevel { Trace, Debug, Info, Warning, Error };
  DECL_ENUM_INFO(LogLevel, LogLevel, "Severity levels for logging messages. Helps categorize and filter log entries based on their importance.", 'logL');
};
}; // namespace shards

#endif // SH_CORE_SHARDS_LOGGING
