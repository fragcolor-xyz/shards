// NOTE: this c++17 feature recently landed in the standard libary but it's not implemented everywhere yet
// https://reviews.llvm.org/rG243da90ea5357c1ca324f714ea4813dc9029af27
#ifdef __APPLE__
#include <experimental/memory_resource>
#include <experimental/__memory>
#include <map>
#include <unordered_map>
#include <set>
#include <vector>
#include <string>
#include <boost/container/pmr/monotonic_buffer_resource.hpp>
namespace std::pmr {
  using namespace std::experimental::pmr;

  template<typename T>
  using vector = std::vector<T, polymorphic_allocator<T>>;

  template<typename K, typename V, typename Less>
  using map = std::map<K, V, Less, polymorphic_allocator<pair<const K, V>>>;

  template<typename K, typename V>
  using unordered_map = std::unordered_map<K, V, polymorphic_allocator<pair<const K, V>>>;

  template<typename K, typename V>
  using set = std::set<K, V, polymorphic_allocator<pair<const K, V>>>;

  template<typename T>
  using string = std::basic_string<char, char_traits<char>, polymorphic_allocator<char>>;
}
#else 
#include <memory_resource>
#endif
