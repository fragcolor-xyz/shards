#ifndef BA11DF4D_49EC_40D2_9BB9_F42CEA1E218C
#define BA11DF4D_49EC_40D2_9BB9_F42CEA1E218C
#include "wrapper.hpp"
#include <unordered_map>

#if !HAVE_CXX_17_MEMORY_RESOURCE
namespace shards::pmr {
template <typename K, typename V, typename Hasher, typename KeyEq>
using unordered_map = std::unordered_map<K, V, Hasher, KeyEq, PolymorphicAllocator<std::pair<const K, V>>>;
}
#endif

#endif /* BA11DF4D_49EC_40D2_9BB9_F42CEA1E218C */
