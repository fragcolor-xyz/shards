#ifndef A2997EDF_4E03_48F4_AF46_10E0BEE1DFE3
#define A2997EDF_4E03_48F4_AF46_10E0BEE1DFE3

#include "boost/core/allocator_access.hpp"
#include "renderer_types.hpp"
#include "pipeline_hash_collector.hpp"
#include "hasherxxh3.hpp"
#include <magic_enum.hpp>
#include <type_traits>
#include <unordered_map>

namespace gfx::detail {

struct PipelineHashCache final : public IPipelineHashStorage {
  using allocator_type = shards::pmr::PolymorphicAllocator<>;

  struct Entry {
    Hash128 hash;
  };

  static inline constexpr size_t NumTypes = magic_enum::enum_count<UniqueIdTag>();
  shards::pmr::unordered_map<UniqueId, Entry> *perType{};

  PipelineHashCache(allocator_type allocator) {
    perType = allocator.new_objects<std::remove_pointer_t<decltype(perType)>>(NumTypes);
  }

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
    for (size_t i = 0; i < NumTypes; i++) {
      auto &lut = perType[i];
      lut.clear();
    }
  }
};

struct PipelineCache {
  std::unordered_map<Hash128, CachedPipelinePtr> map;

  void clear() { map.clear(); }
};

template <typename T> struct is_shared_ptr : std::false_type {};
template <typename T> struct is_shared_ptr<std::shared_ptr<T>> : std::true_type {};

// Checks values in a map for the lastTouched member
// if not used in `frameThreshold` frames, remove it
template <typename T> void clearOldCacheItemsIn(T &iterable, size_t frameCounter, size_t frameThreshold) {
  for (auto it = iterable.begin(); it != iterable.end();) {
    auto &value = it->second;

    using T1 = std::decay_t<decltype(value)>;
    int64_t frameDelta{};
    if constexpr (is_shared_ptr<T1>::value)
      frameDelta = int64_t(frameCounter) - int64_t(value->lastTouched);
    else
      frameDelta = int64_t(frameCounter) - int64_t(value.lastTouched);

    if (frameDelta > int64_t(frameThreshold)) {
      it = iterable.erase(it);
    } else {
      ++it;
    }
  }
}

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
