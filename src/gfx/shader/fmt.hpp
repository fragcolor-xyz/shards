#ifndef GFX_SHADER_FMT
#define GFX_SHADER_FMT

#include "types.hpp"
#include <magic_enum.hpp>
#include <spdlog/fmt/fmt.h>

template <> struct fmt::formatter<gfx::shader::FieldType> {
  constexpr auto parse(format_parse_context &ctx) -> decltype(ctx.begin()) {
    auto it = ctx.begin(), end = ctx.end();
    if (it != end)
      throw format_error("invalid format");
    return it;
  }

  template <typename FormatContext>
  auto format(const gfx::shader::FieldType &fieldType, FormatContext &ctx) -> decltype(ctx.out()) {
    return format_to(ctx.out(), "{{{}, {}}}", magic_enum::enum_name(fieldType.baseType), fieldType.numComponents);
  }
};

#endif // GFX_SHADER_FMT
