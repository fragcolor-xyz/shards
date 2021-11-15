#pragma once

#include <stdexcept>
#include <spdlog/fmt/fmt.h>

namespace gfx {
template <typename... TArgs>
std::exception formatException(const char *format, TArgs... args) {
  return std::runtime_error(fmt::format(format, args...));
}
} // namespace gfx
