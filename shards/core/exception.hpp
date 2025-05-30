#ifndef C87E09D4_EE39_461A_8FD7_741C3396CAB7
#define C87E09D4_EE39_461A_8FD7_741C3396CAB7

#include <spdlog/fmt/fmt.h>
#include <stdexcept>

namespace shards {

template <typename... TArgs> std::runtime_error formatException(fmt::format_string<TArgs...> format, TArgs &&...args) {
  return std::runtime_error(fmt::format(format, std::forward<TArgs>(args)...));
}

} // namespace shards

#endif /* C87E09D4_EE39_461A_8FD7_741C3396CAB7 */
