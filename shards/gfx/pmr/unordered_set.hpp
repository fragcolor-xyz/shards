#ifndef F8A69182_378F_46E3_9942_2FEBE21C2FC4
#define F8A69182_378F_46E3_9942_2FEBE21C2FC4

#include "wrapper.hpp"
#include <functional>
#include <unordered_set>

#if !HAVE_CXX_17_MEMORY_RESOURCE
namespace shards::pmr {
template <typename K, typename Hasher = std::hash<K>, typename Equal = std::equal_to<K>>
using unordered_set = std::unordered_set<K, Hasher, Equal, PolymorphicAllocator<K>>;
}
#endif

#endif /* F8A69182_378F_46E3_9942_2FEBE21C2FC4 */
