#ifndef GFX_SHADER_FMT
#define GFX_SHADER_FMT

#include "types.hpp"
#include "wgsl_mapping.hpp"
#include <magic_enum.hpp>
#include <spdlog/fmt/fmt.h>
#include <variant>

template <> struct fmt::formatter<gfx::shader::NumType> {
  constexpr auto parse(format_parse_context &ctx) -> decltype(ctx.begin()) {
    auto it = ctx.begin(), end = ctx.end();
    if (it != end)
      throw format_error("invalid format");
    return it;
  }

  template <typename FormatContext>
  auto format(const gfx::shader::NumType &fieldType, FormatContext &ctx) -> decltype(ctx.out()) {
    auto baseTypeName = magic_enum::enum_name(fieldType.baseType);
    if (fieldType.matrixDimension > 1) {
      return format_to(ctx.out(), "{{{}, {}x{}}}", baseTypeName, fieldType.numComponents, fieldType.matrixDimension);
    } else {
      return format_to(ctx.out(), "{{{}, {}}}", baseTypeName, fieldType.numComponents);
    }
  }
};

template <> struct fmt::formatter<gfx::shader::Type> {
  constexpr auto parse(format_parse_context &ctx) -> decltype(ctx.begin()) {
    auto it = ctx.begin(), end = ctx.end();
    if (it != end)
      throw format_error("invalid format");
    return it;
  }

  template <typename FormatContext>
  auto format(const gfx::shader::Type &fieldType, FormatContext &ctx) -> decltype(ctx.out()) {
    return std::visit(
        [&](auto &arg) {
          using T = std::decay_t<decltype(arg)>;
          if constexpr (std::is_same_v<T, gfx::shader::NumType>) {
            return format_to(ctx.out(), "{}", arg);
          } else {
            return format_to(ctx.out(), "{{{}}}", gfx::shader::getWGSLTypeName(arg));
          }
        },
        fieldType);
  }
};

#endif // GFX_SHADER_FMT
