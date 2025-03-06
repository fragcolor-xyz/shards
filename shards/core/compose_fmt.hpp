#ifndef E944B5AD_839B_436C_AC6D_3E45828E98A3
#define E944B5AD_839B_436C_AC6D_3E45828E98A3

#include <spdlog/spdlog.h>
#include <spdlog/formatter.h>
#include "ops_internal.hpp"
#include "compose.hpp"

namespace fmt {
// Formatter for VariableAccessor
template <> struct formatter<shards::compose::VariableAccessor> {
  template <typename ParseContext> constexpr auto parse(ParseContext &ctx) { return ctx.begin(); }
  template <typename FormatContext> auto format(shards::compose::VariableAccessor const &va, FormatContext &ctx) {
    if (va.is<shards::compose::VA_Variable>()) {
      auto &va1 = va.as<shards::compose::VA_Variable>();
      if (!va1.debugName.empty()) {
        return format_to(ctx.out(), "{}/{}", va1.debugName, va1.index);
      } else {
        return format_to(ctx.out(), "{}", va1.index);
      }
    } else if (va.is<shards::compose::VA_SubPath>()) {
      return format_to(ctx.out(), "{}", va.as<shards::compose::VA_SubPath>().key);
    } else if (va.is<shards::compose::VA_Input>()) {
      auto &va1 = va.as<shards::compose::VA_Input>();
      return format_to(ctx.out(), "<input:{}>", va1.scopeId);
    } else if (va.is<shards::compose::VA_WireOutput>()) {
      auto &va1 = va.as<shards::compose::VA_WireOutput>();
      if (!va1.debugName.empty()) {
        return format_to(ctx.out(), "<wire:{}/{}>", va1.debugName, va1.wireId);
      } else {
        return format_to(ctx.out(), "<wire:{}>", va1.wireId);
      }
    }
    return format_to(ctx.out(), "unknown");
  }
};

// Formatter for chain as a:b:c format
template <> struct formatter<shards::compose::VariableAccessorChain> {
  template <typename ParseContext> constexpr auto parse(ParseContext &ctx) { return ctx.begin(); }
  template <typename FormatContext> auto format(shards::compose::VariableAccessorChain const &chain, FormatContext &ctx) {
    for (size_t i = 0; i < chain.path.size(); i++) {
      if (i > 0) {
        format_to(ctx.out(), ":");
      }
      format_to(ctx.out(), "{}", chain.path[i]);
    }
    return format_to(ctx.out(), "");
  }
};

template <> struct formatter<shards::compose::VersionedReference> {
  template <typename ParseContext> constexpr auto parse(ParseContext &ctx) { return ctx.begin(); }
  template <typename FormatContext> auto format(shards::compose::VersionedReference const &ref, FormatContext &ctx) {
    return format_to(ctx.out(), "{} (version: {})", ref.chain, ref.version);
  }
};

template <> struct formatter<shards::compose::VariableRef> {
  template <typename ParseContext> constexpr auto parse(ParseContext &ctx) { return ctx.begin(); }
  template <typename FormatContext> auto format(shards::compose::VariableRef const &ref, FormatContext &ctx) {
    if (ref.isValid()) {
      return format_to(ctx.out(), "{}/{} (type: {})", ref->id, ref->exposed.name, ref->exposed.exposedType);
    }
    return format_to(ctx.out(), "unknown");
  }
};
} // namespace fmt

#endif /* E944B5AD_839B_436C_AC6D_3E45828E98A3 */
