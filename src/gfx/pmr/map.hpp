#ifndef D0B19BD7_54AE_4309_BD00_6E46922F7132
#define D0B19BD7_54AE_4309_BD00_6E46922F7132

#include "wrapper.hpp"
#include <map>

#if !HAVE_CXX_17_MEMORY_RESOURCE
namespace shards::pmr {
template <typename K, typename V, typename Less = std::less<K>>
using map = std::map<K, V, Less, PolymorphicAllocator<std::pair<const K, V>>>;
}
#endif

#endif /* D0B19BD7_54AE_4309_BD00_6E46922F7132 */
