#ifndef E2E08FA9_920A_40A9_A223_88E2C8DC7018
#define E2E08FA9_920A_40A9_A223_88E2C8DC7018

#include "shards.hpp"
#include "ops.hpp"
#include <variant>

namespace shards {
struct TableIterator : public std::pair<SHVar, SHVar> {
  const SHTable *table{};
  SHTableIterator it;

  TableIterator() = default;
  TableIterator(const SHTable *table, SHTableIterator it, SHVar k, SHVar v) : std::pair<SHVar, SHVar>(k, v), table(table) {
    memcpy(this->it, it, sizeof(SHTableIterator));
  }
  const TableIterator &operator++() {
    assert(table && table->api && "invalid SHTable for table iterator");
    if(!table->api->tableNext(*table, &it, &std::get<0>(*this), &std::get<1>(*this))) {
      table = nullptr;
    }
    return *this;
  }

  bool operator==(const TableIterator &end) const {
    if (!table && table == end.table)
      return true;
    return std::get<0>(*this) == std::get<0>(end);
  }
  bool operator!=(const TableIterator &end) const {
    return !(*this == end);
  }

  const std::pair<SHVar, SHVar> &operator*() const { return *this; }
  const std::pair<SHVar, SHVar> *operator->() const { return this; }
};
} // namespace shards

inline const SHVar *begin(const SHSeq &a) { return &a.elements[0]; }
inline const SHVar *end(const SHSeq &a) { return begin(a) + a.len; }
inline shards::TableIterator begin(const SHTable &a) {
  SHTableIterator it{};
  SHVar k{};
  SHVar v{};
  a.api->tableGetIterator(a, &it);
  a.api->tableNext(a, &it, &k, &v);
  return shards::TableIterator(&a, it, k, v);
}
inline shards::TableIterator end(const SHTable &a) { return shards::TableIterator(); }

#endif /* E2E08FA9_920A_40A9_A223_88E2C8DC7018 */
