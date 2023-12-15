#ifndef CD67ABC4_DB1A_499D_B19A_DF8B82F5AB54
#define CD67ABC4_DB1A_499D_B19A_DF8B82F5AB54

#include "fast_string.hpp"
#include <spdlog/fmt/fmt.h>

template <class T, int M> struct fmt::formatter<shards::fast_string::FastString> {
  constexpr auto parse(format_parse_context &ctx) -> decltype(ctx.begin()) {
    auto it = ctx.begin(), end = ctx.end();
    if (it != end)
      throw format_error("invalid format");
    return it;
  }

  template <typename FormatContext> auto format(const shards::fast_string::FastString &str, FormatContext &ctx) -> decltype(ctx.out()) {
    return format_to(ctx.out(), "{}", str.str());
  }
};

#endif /* F69DB8FE_C060_498E_914A_E5245FB65749 */


#endif /* CD67ABC4_DB1A_499D_B19A_DF8B82F5AB54 */
