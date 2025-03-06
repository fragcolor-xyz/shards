#ifndef F00F35A8_DC9E_47A6_BBE0_C30732D42D0E
#define F00F35A8_DC9E_47A6_BBE0_C30732D42D0E

#include "wrapper.hpp"
#include <functional>
#include <set>

#if !HAVE_CXX_17_MEMORY_RESOURCE
namespace shards::pmr {
template <typename K, typename Less = std::less<K>> using set = std::set<K, Less, PolymorphicAllocator<K>>;
}
#endif

#endif /* F00F35A8_DC9E_47A6_BBE0_C30732D42D0E */
