#ifndef E944B5AD_839B_436C_AC6D_3E45828E98A3
#define E944B5AD_839B_436C_AC6D_3E45828E98A3

#include "compose.hpp"

namespace fmt {
template <> struct formatter<shards::compose::AssertionRule> {
  template <typename ParseContext> constexpr auto parse(ParseContext &ctx) { return ctx.begin(); }
  template <typename FormatContext> auto format(const shards::compose::AssertionRule &r, FormatContext &ctx) {
    if (auto *v = std::get_if<shards::compose::AssertionRule_IsA>(&r)) {
      return format_to(ctx.out(), "IsA({})", *v->type);
    } else if (auto *v = std::get_if<shards::compose::AssertionRule_IsNot>(&r)) {
      return format_to(ctx.out(), "IsNot({})", *v->type);
    } else {
      return format_to(ctx.out(), "Unknown");
    }
  }
};
template <> struct formatter<shards::compose::AssertionRules> {
  template <typename ParseContext> constexpr auto parse(ParseContext &ctx) { return ctx.begin(); }
  template <typename FormatContext> auto format(const shards::compose::AssertionRules &r, FormatContext &ctx) {
    return format_to(ctx.out(), "{}", fmt::join(r.matches, " && "));
  }
};
} // namespace fmt

#endif /* E944B5AD_839B_436C_AC6D_3E45828E98A3 */
