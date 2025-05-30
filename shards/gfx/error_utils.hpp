#ifndef GFX_ERROR_UTILS
#define GFX_ERROR_UTILS

#include <shards/core/exception.hpp>
#include <spdlog/fmt/fmt.h>
#include <stdexcept>

namespace gfx {
template <typename... TArgs> std::runtime_error formatException(fmt::format_string<TArgs...> format, TArgs &&...args) {
  return shards::formatException(format, std::forward<TArgs>(args)...);
}
} // namespace gfx

#endif // GFX_ERROR_UTILS
