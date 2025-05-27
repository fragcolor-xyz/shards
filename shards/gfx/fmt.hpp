#ifndef F69DB8FE_C060_498E_914A_E5245FB65749
#define F69DB8FE_C060_498E_914A_E5245FB65749

#include "linalg.hpp"
#include "unique_id.hpp"
#include "gfx_wgpu.hpp"
#include <magic_enum.hpp>
#include <spdlog/fmt/fmt.h>
#include <sstream>

template <class T, int M> struct fmt::formatter<linalg::vec<T, M>> : fmt::formatter<std::string_view> {
  template <typename FormatContext> auto format(const linalg::vec<T, M> &vec, FormatContext &ctx) const -> decltype(ctx.out()) {
    using namespace linalg::ostream_overloads;
    std::stringstream ss;
    ss << vec;
    return format_to(ctx.out(), "{}", ss.str());
  }
};

template <> struct fmt::formatter<gfx::UniqueId> : fmt::formatter<std::string_view> {
  template <typename FormatContext> auto format(const gfx::UniqueId &id, FormatContext &ctx) const -> decltype(ctx.out()) {
    return format_to(ctx.out(), "{}", id.value);
  }
};

inline auto format_as(WGPUTextureFormat texture) {
  return magic_enum::enum_name(texture);
}

#endif /* F69DB8FE_C060_498E_914A_E5245FB65749 */
