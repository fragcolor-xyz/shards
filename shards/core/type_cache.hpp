#ifndef D0E98483_F0A4_476F_81F3_8709C5D852D7
#define D0E98483_F0A4_476F_81F3_8709C5D852D7

#include "hash.inl"
#include "foundation.hpp"
#include <map>
#include <shared_mutex>

namespace shards {
// Storage for TypeInfo that is generated dynamically and might be passed outside of compose
struct TypeCache {
private:
  std::map<XXH128_hash_t, shards::TypeInfo *> types;
  std::shared_mutex mutex;

public:
  TypeCache() = default;
  TypeCache(const TypeCache &) = delete;
  TypeCache &operator=(const TypeCache &) = delete;
  ~TypeCache() {
    for (auto &it : types) {
      delete it.second;
    }
  }

  const SHTypeInfo &insertUnique(shards::TypeInfo &&ti) {
    shards::HashState<XXH128_hash_t> hs;
    auto hash = hs.deriveTypeHash(ti);

    std::shared_lock<decltype(mutex)> lock(mutex);
    auto it = types.find(hash);
    if (it == types.end()) {
      lock.unlock();
      std::unique_lock<decltype(mutex)> lock(mutex);
      it = types.find(hash); // Need to double check after re-lock
      if (it != types.end()) {
        return (SHTypeInfo &)*it->second;
      }

      it = types.emplace(hash, new shards::TypeInfo(std::move(ti))).first;
      return (SHTypeInfo &)*it->second;
    }
    return (SHTypeInfo &)*it->second;
  }
  
  static TypeCache &instance() {
    static TypeCache tc;
    return tc;
  }
};
} // namespace shards

#endif /* D0E98483_F0A4_476F_81F3_8709C5D852D7 */
