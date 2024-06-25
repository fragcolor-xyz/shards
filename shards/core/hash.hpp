#ifndef B85EE7A9_531D_475D_ACBB_35F5AD163CC9
#define B85EE7A9_531D_475D_ACBB_35F5AD163CC9

#include <shards/shards.h>
#include <unordered_set>
#include <set>
#include <unordered_map>

#define XXH_INLINE_ALL
#include <xxhash.h>

namespace std {
template <> struct less<XXH128_hash_t> {
  bool operator()(const XXH128_hash_t &lhs, const XXH128_hash_t &rhs) const {
    return lhs.high64 < rhs.high64 || (lhs.high64 == rhs.high64 && lhs.low64 < rhs.low64);
  }
};
} // namespace std

namespace shards {
template <typename TDigest> struct HashState {
  std::vector<std::set<TDigest>> hashSets;
  std::unordered_map<SHWire *, TDigest> wireHashes;
  std::vector<SHWire *> wireStack;
  int hashSetCounter{};
  int depth{};

  void reset() { wireHashes.clear(); }

  void updateTypeHash(const SHVar &var, XXH3_state_s *state);
  void updateTypeHash(const SHTypeInfo &t, XXH3_state_s *state);
  void updateHash(const SHVar &t, XXH3_state_s *state);

  TDigest deriveTypeHash(const SHVar &var);
  TDigest deriveTypeHash(const SHTypeInfo &var);
  TDigest hash(const SHVar &var);
  TDigest hash(const std::shared_ptr<SHWire> &var);
};

uint64_t deriveTypeHash64(const SHVar &var);
uint64_t deriveTypeHash64(const SHTypeInfo &var);

template <typename TDigest> void hashReset(XXH3_state_t *state) {
  if constexpr (sizeof(TDigest) == 8) {
    XXH3_64bits_reset(state);
  } else {
    XXH3_128bits_reset(state);
  }
}
template <typename TDigest> TDigest hashDigest(const XXH3_state_t *state) {
  if constexpr (sizeof(TDigest) == 8) {
    return XXH3_64bits_digest(state);
  } else {
    return XXH3_128bits_digest(state);
  }
}
template <typename TDigest> XXH_errorcode hashUpdate(XXH3_state_t *state, const void *input, size_t len) {
  if constexpr (sizeof(TDigest) == 8) {
    return XXH3_64bits_update(state, input, len);
  } else {
    return XXH3_128bits_update(state, input, len);
  }
}

} // namespace shards

#endif /* B85EE7A9_531D_475D_ACBB_35F5AD163CC9 */
