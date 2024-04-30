/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2019 Fragcolor Pte. Ltd. */

#ifndef SH_CORE_OPS_INTERNAL
#define SH_CORE_OPS_INTERNAL

#include "magic_enum.hpp"
#include <shards/shards.hpp>
#include "spdlog/fmt/bundled/core.h"
#include <shards/ops.hpp>
#include <sstream>
#include <spdlog/fmt/fmt.h>
#include <spdlog/fmt/ostr.h> // must be included

namespace shards {
struct DocsFriendlyFormatter {
  bool ignoreNone{};

  std::ostream &format(std::ostream &os, const SHStringWithLen &var);
  std::ostream &format(std::ostream &os, const SHVar &var);
  std::ostream &format(std::ostream &os, const SHTypeInfo &var);
  std::ostream &format(std::ostream &os, const SHTypesInfo &var);
  std::ostream &format(std::ostream &os, const SHTrait &var);
};

static inline DocsFriendlyFormatter defaultFormatter{};
} // namespace shards

inline std::ostream &operator<<(std::ostream &os, const SHStringWithLen &v) { return shards::defaultFormatter.format(os, v); }
inline std::ostream &operator<<(std::ostream &os, const SHVar &v) { return shards::defaultFormatter.format(os, v); }
inline std::ostream &operator<<(std::ostream &os, const SHTypeInfo &v) { return shards::defaultFormatter.format(os, v); }
inline std::ostream &operator<<(std::ostream &os, const SHTypesInfo &v) { return shards::defaultFormatter.format(os, v); }
inline std::ostream &operator<<(std::ostream &os, const SHTrait &v) { return shards::defaultFormatter.format(os, v); }

template <typename T> struct StringStreamFormatter {
  constexpr auto parse(fmt::format_parse_context &ctx) -> decltype(ctx.begin()) {
    auto it = ctx.begin(), end = ctx.end();
    if (it != end)
      throw fmt::format_error("invalid format");
    return it;
  }

  template <typename FormatContext> auto format(const T &v, FormatContext &ctx) -> decltype(ctx.out()) {
    std::stringstream ss;
    ss << v;
    return fmt::format_to(ctx.out(), "{}", ss.str());
  }
};

template <> struct fmt::formatter<SHStringWithLen> {
  StringStreamFormatter<SHStringWithLen> base;
  constexpr auto parse(format_parse_context &ctx) -> decltype(ctx.begin()) { return base.parse(ctx); }
  template <typename FormatContext> auto format(const SHStringWithLen &v, FormatContext &ctx) -> decltype(ctx.out()) {
    return base.format(v, ctx);
  }
};

template <> struct fmt::formatter<SHTrait> {
  StringStreamFormatter<SHTrait> base;
  constexpr auto parse(format_parse_context &ctx) -> decltype(ctx.begin()) { return base.parse(ctx); }
  template <typename FormatContext> auto format(const SHTrait &v, FormatContext &ctx) -> decltype(ctx.out()) {
    return base.format(v, ctx);
  }
};

template <> struct fmt::formatter<SHVar> {
  StringStreamFormatter<SHVar> base;
  constexpr auto parse(format_parse_context &ctx) -> decltype(ctx.begin()) { return base.parse(ctx); }
  template <typename FormatContext> auto format(const SHVar &v, FormatContext &ctx) -> decltype(ctx.out()) {
    return base.format(v, ctx);
  }
};

template <> struct fmt::formatter<SHTypeInfo> {
  StringStreamFormatter<SHTypeInfo> base;
  constexpr auto parse(format_parse_context &ctx) -> decltype(ctx.begin()) { return base.parse(ctx); }
  template <typename FormatContext> auto format(const SHTypeInfo &v, FormatContext &ctx) -> decltype(ctx.out()) {
    return base.format(v, ctx);
  }
};

template <> struct fmt::formatter<SHTypesInfo> {
  StringStreamFormatter<SHTypesInfo> base;
  constexpr auto parse(format_parse_context &ctx) -> decltype(ctx.begin()) { return base.parse(ctx); }
  template <typename FormatContext> auto format(const SHTypesInfo &v, FormatContext &ctx) -> decltype(ctx.out()) {
    return base.format(v, ctx);
  }
};

template <> struct fmt::formatter<SHType> {
  constexpr auto parse(format_parse_context &ctx) -> decltype(ctx.begin()) {
    auto it = ctx.begin(), end = ctx.end();
    if (it != end)
      throw format_error("invalid format");
    return it;
  }
  template <typename FormatContext> auto format(const SHType &v, FormatContext &ctx) -> decltype(ctx.out()) {
    return format_to(ctx.out(), "{}", magic_enum::enum_name(v));
  }
};

template <> struct fmt::formatter<shards::Type> {
  fmt::formatter<SHTypeInfo> base;
  constexpr auto parse(format_parse_context &ctx) -> decltype(ctx.begin()) { return base.parse(ctx); }
  template <typename FormatContext> auto format(const shards::Type &v, FormatContext &ctx) -> decltype(ctx.out()) {
    return base.format(v, ctx);
  }
};

template <> struct fmt::formatter<shards::Types> {
  fmt::formatter<SHTypesInfo> base;
  constexpr auto parse(format_parse_context &ctx) -> decltype(ctx.begin()) { return base.parse(ctx); }
  template <typename FormatContext> auto format(const shards::Types &v, FormatContext &ctx) -> decltype(ctx.out()) {
    return base.format(v, ctx);
  }
};

#endif // SH_CORE_OPS_INTERNAL
