#ifndef A67E2D4D_B267_4EFB_9A3C_C2BF0D2FE420
#define A67E2D4D_B267_4EFB_9A3C_C2BF0D2FE420

#include <vector>
#include <stdexcept>
#include <spdlog/fmt/fmt.h>

namespace gfx {

template <typename T> inline void ensureEmptyStack(std::vector<T> &stack) {
  size_t numItems = stack.size();
  if (numItems != 0) {
    stack.clear();
    throw std::runtime_error(fmt::format("Stack was not empty({} elements not popped)", numItems));
  }
}
} // namespace gfx

#endif /* A67E2D4D_B267_4EFB_9A3C_C2BF0D2FE420 */
