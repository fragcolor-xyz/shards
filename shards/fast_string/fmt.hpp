#ifndef C15A8B87_4A27_4D56_AD9E_4DA2AACA21FE
#define C15A8B87_4A27_4D56_AD9E_4DA2AACA21FE

#include "fast_string.hpp"
#include <spdlog/fmt/fmt.h>

// template<> struct fmt::formatter<shards::fast_string::FastString> {
//   constexpr auto parse(format_parse_context &ctx) -> decltype(ctx.begin()) {
//     auto it = ctx.begin(), end = ctx.end();
//     if (it != end)
//       throw format_error("invalid format");
//     return it;
//   }

//   template <typename FormatContext>
//   auto format(const shards::fast_string::FastString &str, FormatContext &ctx) -> decltype(ctx.out()) {
//     return format_to(ctx.out(), "{}", str.str());
//   }
// };

#endif /* C15A8B87_4A27_4D56_AD9E_4DA2AACA21FE */
