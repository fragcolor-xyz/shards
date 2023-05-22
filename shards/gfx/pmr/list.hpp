#ifndef EC1367EE_0ED3_4E88_9594_AB1EEF37C0C0
#define EC1367EE_0ED3_4E88_9594_AB1EEF37C0C0

#include "wrapper.hpp"
#include <list>

#if !HAVE_CXX_17_MEMORY_RESOURCE
namespace shards::pmr {
template <typename T> using list = std::list<T, PolymorphicAllocator<T>>;
}
#endif

#endif /* EC1367EE_0ED3_4E88_9594_AB1EEF37C0C0 */
