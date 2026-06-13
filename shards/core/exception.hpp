#ifndef C87E09D4_EE39_461A_8FD7_741C3396CAB7
#define C87E09D4_EE39_461A_8FD7_741C3396CAB7

#include <spdlog/fmt/fmt.h>
#include <stdexcept>

namespace shards {

template <typename S, typename... TArgs> std::runtime_error formatException(const S &format, TArgs &&...args) {
  return std::runtime_error(fmt::format(fmt::runtime(format), std::forward<TArgs>(args)...));
}

} // namespace shards

#endif /* C87E09D4_EE39_461A_8FD7_741C3396CAB7 */
