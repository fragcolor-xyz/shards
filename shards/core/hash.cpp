#include "hash.inl"

namespace shards {
uint64_t deriveTypeHash64(const SHVar &var) {
  static thread_local HashState<XXH64_hash_t> h;
  h.reset();
  return h.deriveTypeHash(var);
}
uint64_t deriveTypeHash64(const SHTypeInfo &t) {
  static thread_local HashState<XXH64_hash_t> h;
  h.reset();
  return h.deriveTypeHash(t);
}
} // namespace shards