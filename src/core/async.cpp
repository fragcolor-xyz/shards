#include "async.hpp"

#if HAS_ASYNC_SUPPORT
namespace shards {
TidePool &getTidePool() {
  static TidePool tidePool;
  return tidePool;
}
} // namespace shards
#endif
