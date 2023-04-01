#include "async.hpp"
#include "shards/flow.hpp"
#include <atomic>
#include <chrono>
#include <cstddef>
#include <thread>
#include <future>
#include <vector>
#include <boost/lockfree/queue.hpp>

#if HAS_ASYNC_SUPPORT
namespace shards {
TidePool &getTidePool() {
  static TidePool tidePool;
  return tidePool;
}
} // namespace shards
#endif
