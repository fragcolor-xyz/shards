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
    auto baseTypeName = magic_enum::enum_name(fieldType.baseType);
    if (fieldType.matrixDimension > 1) {
      return format_to(ctx.out(), "{{{}, {}x{}}}", baseTypeName, fieldType.numComponents, fieldType.matrixDimension);
    } else {
      return format_to(ctx.out(), "{{{}, {}}}", baseTypeName, fieldType.numComponents);
    }
  }
};

#endif // GFX_SHADER_FMT
