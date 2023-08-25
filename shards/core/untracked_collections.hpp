#ifndef C00AFEDF_4760_4B00_93F3_8C80A9784211
#define C00AFEDF_4760_4B00_93F3_8C80A9784211

#include <vector>
#include <list>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <boost/container/allocator.hpp>

namespace shards {
// Collections not using the default new/delete allocator so that they can be used outside of tracy lifetime
template <typename K, typename V>
using UntrackedUnorderedMap =
    std::unordered_map<K, V, std::hash<K>, std::equal_to<K>, boost::container::allocator<std::pair<const K, V>>>;
template <typename K, typename V>
using UntrackedMap = std::map<K, V, std::less<K>, boost::container::allocator<std::pair<const K, V>>>;
template <typename V>
using UntrackedUnorderedSet = std::unordered_set<V, std::hash<V>, std::equal_to<V>, boost::container::allocator<V>>;
template <typename V> using UntrackedList = std::list<V, boost::container::allocator<V>>;
template <typename V> using UntrackedVector = std::vector<V, boost::container::allocator<V>>;
using UntrackedString = std::basic_string<char, std::char_traits<char>, boost::container::allocator<char>>;
}

#endif /* C00AFEDF_4760_4B00_93F3_8C80A9784211 */
