#pragma once

#include <spdlog/fmt/fmt.h>
#include <stdexcept>

namespace gfx {
template <typename... TArgs> std::runtime_error formatException(const char *format, TArgs... args) {
  return std::runtime_error(fmt::format(format, args...));
}
} // namespace gfx
