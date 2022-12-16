#ifndef A2997EDF_4E03_48F4_AF46_10E0BEE1DFE3
#define A2997EDF_4E03_48F4_AF46_10E0BEE1DFE3

#include "renderer_types.hpp"
#include "pipeline_hash_collector.hpp"
#include "hasherxxh128.hpp"
#include <magic_enum.hpp>
#include <unordered_map>

namespace gfx::detail {

struct PipelineHashCache final : public IPipelineHashStorage {
  struct Entry {
    Hash128 hash;
  };
  std::unordered_map<UniqueId, Entry> perType[magic_enum::enum_count<UniqueIdTag>()];

  const Hash128 *find(UniqueId id) {
    auto &lut = perType[(uint8_t)id.getTag()];
    auto it = lut.find(id);
    if (it != lut.end()) {
      return &it->second.hash;
    } else {
      return nullptr;
    }
  }

  template <typename T> const Hash128 &update(const std::shared_ptr<T> &hashable) { return update(*hashable.get()); }

  template <typename T> const Hash128 &update(const T &hashable) {
    UniqueId id = hashable.getId();
    PipelineHashCollector collector{.storage = this};
    hashable.pipelineHashCollect(collector);

    Hash128 hash = collector.hasher.getDigest();

    auto &lut = perType[(uint8_t)id.getTag()];
    auto it = lut.find(id);
    if (it != lut.end()) {
      it->second.hash = hash;
    } else {
      it = lut.insert(std::make_pair(id, Entry{.hash = hash})).first;
    }

    return it->second.hash;
  }

  virtual std::optional<Hash128> getHash(UniqueId id) {
    auto hash = find(id);
    return hash ? std::make_optional(*hash) : std::nullopt;
  }

  virtual void addHash(UniqueId id, Hash128 hash) {
    auto &lut = perType[(uint8_t)id.getTag()];
    lut.insert(std::make_pair(id, Entry{.hash = hash}));
  }

  void reset() {
    for (size_t i = 0; i < std::size(perType); i++) {
      auto &lut = perType[i];
      lut.clear();
    }
  }
};

struct PipelineCache {
  std::unordered_map<Hash128, CachedPipelinePtr> map;

  void clear() { map.clear(); }
};

template <typename TCache, typename K, typename TInit>
typename TCache::mapped_type &getCacheEntry(TCache &cache, const K &key, TInit &&init) {
  auto it = cache.find(key);
  if (it == cache.end()) {
    it = cache.insert(std::make_pair(key, init(key))).first;
  }
  return it->second;
}

template <typename TCache, typename K> typename TCache::mapped_type &getCacheEntry(TCache &cache, const K &key) {
  return getCacheEntry(cache, key, [](const K &key) { return typename TCache::mapped_type(); });
}

template <typename TElem, typename TCache, typename K> std::shared_ptr<TElem> &getSharedCacheEntry(TCache &cache, const K &key) {
  return getCacheEntry(cache, key, [](const K &key) { return std::make_shared<TElem>(); });
}

template <typename T> void touchCacheItem(T &item, size_t frameCounter) { item.lastTouched = frameCounter; }
template <typename T> void touchCacheItemPtr(std::shared_ptr<T> &item, size_t frameCounter) {
  touchCacheItem(*item.get(), frameCounter);
}

} // namespace gfx::detail

#endif /* A2997EDF_4E03_48F4_AF46_10E0BEE1DFE3 */
