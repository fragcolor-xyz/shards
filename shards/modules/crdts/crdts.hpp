#ifndef SHARDS_CRDTS_HPP
#define SHARDS_CRDTS_HPP

#include <shards/shards.hpp>
#include <shards/utility.hpp>
#include <shards/core/shared.hpp>
#include <shards/core/params.hpp>
#include <shards/common_types.hpp>
#include <boost/unordered/unordered_flat_map.hpp>
#include <boost/unordered/unordered_flat_set.hpp>
#include <boost/container/small_vector.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/nil_generator.hpp>

struct CrdtKey {
  CrdtKey(std::string_view name) : name(name) {}
  CrdtKey(std::string_view name, shards::OwnedVar key)
      : name(name), key(key->isNone() ? std::nullopt : std::make_optional(key)) {}

  // Template constructor for const char[N], enabled only when N > 1
  template <std::size_t N, typename = std::enable_if_t<(N > 1)>>
  CrdtKey(const char (&src)[N]) : CrdtKey(std::string_view(src, N - 1)) {}

  // Constructor for empty string literals
  CrdtKey(const char (&src)[1]) : CrdtKey(std::string_view(src, 0)) {}

  bool operator==(const CrdtKey &other) const { return name == other.name && key == other.key; }
  bool operator==(const std::string_view &other) const { return name == other; }
  template <std::size_t N, typename = std::enable_if_t<(N > 1)>> bool operator==(const char (&other)[N]) const {
    return name == std::string_view(other, N - 1);
  }

  // less than operator for behavior key
  bool operator<(const CrdtKey &other) const { return std::tie(name, key) < std::tie(other.name, other.key); }

public:
  std::string name;
  std::optional<shards::OwnedVar> key;
};

// CrdtKey hasher
namespace std {
template <> struct hash<CrdtKey> {
  std::size_t operator()(const CrdtKey &key) const {
    std::size_t result = std::hash<std::string>{}(key.name);
    if (key.key) {
      result ^= std::hash<shards::OwnedVar>{}(*key.key);
    }
    return result;
  }
};
} // namespace std

using CrdtNodeId = boost::uuids::uuid; // Int16/uuid
// this seems the best combination for containers btw!, we tried boost unordered_flat_map for the rest but it was slower
template <typename T> using CrdtVector = boost::container::small_vector<T, 4>;
template <typename K, typename V> using CrdtMap = std::unordered_map<K, V>;
template <typename K> using CrdtSet = std::unordered_set<K, std::hash<K>>;
template <typename T, typename Comparator> using CrdtSortedSet = boost::container::flat_set<T, Comparator>;
template <typename K, typename V> using CrdtTombstoneMap = boost::unordered_flat_map<K, V, std::hash<K>>;

#define CRDT_COLLECTIONS_DEFINED
#include "crdt.hpp"

namespace shards {
namespace crdts {
#define FIELDS                       \
  FIXED_TABLE_FIELD(col_name, 0)     \
  FIXED_TABLE_FIELD(col_name_key, 1) \
  FIXED_TABLE_FIELD(col_version, 2)  \
  FIXED_TABLE_FIELD(db_version, 3)   \
  FIXED_TABLE_FIELD(flags, 4)        \
  FIXED_TABLE_FIELD(node_id, 5)      \
  FIXED_TABLE_FIELD(record_id, 6)    \
  FIXED_TABLE_FIELD(value, 7)

DEFINE_FIXED_TABLE(ChangesFixedTable,                  //
                   insert("col-name", Var::Empty);     //
                   insert("col-name-key", Var::Empty); //
                   insert("col-version", Var::Empty);  //
                   insert("db-version", Var::Empty);   //
                   insert("flags", Var::Empty);        //
                   insert("node-id", Var::Empty);      //
                   insert("record-id", Var::Empty);    //
                   insert("value", Var::Empty);)
#undef FIELDS
} // namespace crdts
} // namespace shards

#endif // SHARDS_CRDTS_HPP