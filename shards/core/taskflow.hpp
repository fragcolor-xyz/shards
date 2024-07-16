#ifndef A036F6B0_2047_4DD1_B2FB_BD670D0373F7
#define A036F6B0_2047_4DD1_B2FB_BD670D0373F7

// Remove define from winspool.h
#ifdef MAX_PRIORITY
#undef MAX_PRIORITY
#endif

#include <taskflow/taskflow.hpp>
#include <taskflow/algorithm/for_each.hpp>

namespace shards {
struct TaskFlowInstance {
  static tf::Executor& instance();
};
} // namespace shards::async

#endif /* A036F6B0_2047_4DD1_B2FB_BD670D0373F7 */
