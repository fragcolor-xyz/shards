/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2019 Fragcolor Pte. Ltd. */

#ifndef SH_CORE_OPS_INTERNAL
#define SH_CORE_OPS_INTERNAL

#include <ops.hpp>

#include <spdlog/spdlog.h>

#include "spdlog/fmt/ostr.h" // must be included

namespace shards {
struct DocsFriendlyFormatter {
  bool ignoreNone{};

  std::ostream &format(std::ostream &os, const SHVar &var);
  std::ostream &format(std::ostream &os, const SHTypeInfo &var);
  std::ostream &format(std::ostream &os, const SHTypesInfo &var);
};

static inline DocsFriendlyFormatter defaultFormatter{};
} // namespace shards

inline std::ostream &operator<<(std::ostream &os, const SHVar &v) { return shards::defaultFormatter.format(os, v); }
inline std::ostream &operator<<(std::ostream &os, const SHTypeInfo &v) { return shards::defaultFormatter.format(os, v); }
inline std::ostream &operator<<(std::ostream &os, const SHTypesInfo &v) { return shards::defaultFormatter.format(os, v); }

#endif // SH_CORE_OPS_INTERNAL
