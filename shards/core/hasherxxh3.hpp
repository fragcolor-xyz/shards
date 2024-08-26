#ifndef B36CD6BA_CB51_428C_BA06_73B93E88A318
#define B36CD6BA_CB51_428C_BA06_73B93E88A318

#include <linalg.h>
#include <shards/core/hash.hpp>
#include <shards/fast_string/fast_string.hpp>
#include <optional>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>
#include <concepts>

namespace shards {
struct Hash128 {
  uint64_t low = 0, high = 0;
  Hash128() = default;
  Hash128(XXH128_hash_t hash) : low(hash.low64), high(hash.high64) {}
  Hash128(uint64_t low, uint64_t high) : low(low), high(high) {}
  std::strong_ordering operator<=>(const Hash128 &) const = default;
  operator XXH128_hash_t() const { return XXH128_hash_t{low, high}; }
};
} // namespace shards

template <> struct std::hash<shards::Hash128> {
  size_t operator()(const shards::Hash128 &h) const { return size_t(h.low); }
};


namespace shards {
struct DummyHasher {
  template <typename T> void operator()(const T &) {}
  void operator()(const void *data, size_t length) {}
};

template <typename T>
concept TriviallyHashable = std::is_trivial_v<T>;

template <typename T, typename TVisitor>
concept VisitorHashable = !TriviallyHashable<T> && requires(T value, TVisitor visitor, DummyHasher hasher) { visitor(value, hasher); };

struct HasherDefaultVisitor {
  template <typename T, typename THasher> void operator()(const T &value, THasher &&hasher) { value.hash(hasher); }
};

template <typename TVisitor = HasherDefaultVisitor, typename TDigest = XXH128_hash_t> struct HasherXXH3 {
  XXH3_state_t state = {};

  HasherXXH3() {
    XXH3_INITSTATE(&state);
    reset();
  }

  void reset() { shards::hashReset<TDigest>(&state); }

  TDigest getDigest() const { return shards::hashDigest<TDigest>(&state); }

  void operator()(const void *data, size_t length) { shards::hashUpdate<TDigest>(&state, data, length); }

  void operator()(const XXH128_hash_t &v) { (*this)(&v, sizeof(XXH128_hash_t)); }
  void operator()(const XXH64_hash_t &v) { (*this)(&v, sizeof(XXH64_hash_t)); }

  template <typename T, int N> void operator()(const linalg::vec<T, N> &v) { (*this)(&v.x, sizeof(T) * N); }

  template <typename TVal> void operator()(const std::optional<TVal> &v) {
    bool has_value = v.has_value();
    (*this)(uint8_t(has_value));
    if (has_value)
      (*this)(v.value());
  }

  template <TriviallyHashable TVal> void operator()(const TVal &val) { (*this)(&val, sizeof(val)); }

  template <VisitorHashable<TVisitor> TVal> void operator()(const TVal &val) { TVisitor{}(val, *this); }

  void operator()(const std::string &str) { (*this)(str.data(), str.size()); }
  void operator()(const shards::fast_string::FastString &str) { (*this)(str.id); }

  template <typename TVal> void operator()(const std::vector<TVal> &vec) {
    for (auto &item : vec) {
      (*this)(item);
    }
  }

  template <typename TVal> void operator()(const std::unordered_map<std::string, TVal> &map) {
    for (auto &pair : map) {
      (*this)(pair.first);
      (*this)(pair.second);
    }
  }
};

} // namespace shards

#endif /* B36CD6BA_CB51_428C_BA06_73B93E88A318 */
