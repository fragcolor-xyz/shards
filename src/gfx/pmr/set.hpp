#ifndef F8A69182_378F_46E3_9942_2FEBE21C2FC4
#define F8A69182_378F_46E3_9942_2FEBE21C2FC4

#include "wrapper.hpp"
#include <functional>
#include <set>

#if !HAVE_CXX_17_MEMORY_RESOURCE
namespace shards::pmr {
template <typename K, typename Less = std::less<K>> using set = std::set<K, Less, PolymorphicAllocator<K>>;
}
#endif

#endif /* F8A69182_378F_46E3_9942_2FEBE21C2FC4 */
