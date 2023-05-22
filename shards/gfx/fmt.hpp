#ifndef F69DB8FE_C060_498E_914A_E5245FB65749
#define F69DB8FE_C060_498E_914A_E5245FB65749

#include "linalg.hpp"
#include <spdlog/fmt/fmt.h>
#include <sstream>

template <class T, int M> struct fmt::formatter<linalg::vec<T, M>> {
  constexpr auto parse(format_parse_context &ctx) -> decltype(ctx.begin()) {
    auto it = ctx.begin(), end = ctx.end();
    if (it != end)
      throw format_error("invalid format");
    return it;
  }

  template <typename FormatContext> auto format(const linalg::vec<T, M> &vec, FormatContext &ctx) -> decltype(ctx.out()) {
    using namespace linalg::ostream_overloads;
    std::stringstream ss;
    ss << vec;
    return format_to(ctx.out(), "{}", ss.str());
  }
};

#endif /* F69DB8FE_C060_498E_914A_E5245FB65749 */
