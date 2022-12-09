#ifndef A2997EDF_4E03_48F4_AF46_10E0BEE1DFE3
#define A2997EDF_4E03_48F4_AF46_10E0BEE1DFE3

#include "renderer_types.hpp"
#include "pipeline_hash_collector.hpp"
#include "hasherxxh128.hpp"
#include <magic_enum.hpp>
#include <unordered_map>

namespace gfx::detail {

struct PipelineCacheUpdate {
  std::set<UniqueId> evictedEntries;

  void reset() { evictedEntries.clear(); }
};

struct PipelineCache1 {
  struct Entry {
    References references;
    Hash128 hash;
  };
  std::unordered_map<UniqueId, Entry> perType[magic_enum::enum_count<UniqueIdTag>()];
  std::set<UniqueId> evictionQueue;

  const Entry *find(UniqueId id) {
    auto &lut = perType[(uint8_t)id.getTag()];
    auto it = lut.find(id);
    if (it != lut.end()) {
      return &it->second;
    } else {
      return nullptr;
    }
  }

  void evict(UniqueId _id, PipelineCacheUpdate &update) {
    evictionQueue.clear();
    evictionQueue.insert(_id);

    // Remove entries that reference this id
    while (!evictionQueue.empty()) {
      UniqueId id = evictionQueue.begin()->value;
      evictionQueue.erase(id);

      for (size_t i = 0; i < std::size(perType); i++) {
        auto &lut = perType[i];
        for (auto it = lut.begin(); it != lut.end(); ++it) {
          if (it->second.references.contains(id)) {
            // Add to the queue, so that items that reference this item also get evicted
            evictionQueue.insert(it->first);
          }
        }
      }

      // Remove the originally evicted id
      auto &lut = perType[(uint8_t)id.getTag()];
      if (lut.erase(id) > 0)
        update.evictedEntries.insert(id);
    }
  }

  template <typename T> void update(const T &hashable, PipelineCacheUpdate &update) {
    update.reset();

    UniqueId id = hashable.getId();
    PipelineHashCollector collector;
    hashable.pipelineHashCollect(collector);

    Hash128 hash = collector.hasher.getDigest();

    auto &lut = perType[(uint8_t)id.getTag()];
    auto it = lut.find(id);
    if (it != lut.end()) {
      if (it->second.hash != hash) {
        evict(id, update);
      } else {
        return; // Up-to-date
      }
    } else {
      evict(id, update);
    }

    lut.insert(std::make_pair(id, Entry{
                                      .references = std::move(collector.references),
                                      .hash = hash,
                                  }));
  }
};

struct PipelineCache {
  std::unordered_map<Hash128, CachedPipelinePtr> map;

  void clear() { map.clear(); }
};

// Associates drawables with the render pipeline they belong to
struct PipelineDrawableCache {
  std::unordered_map<Hash128, PipelineDrawables> map;

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

template <typename TElem, typename TCache, typename K> std::shared_ptr<TElem> &getSharedCacheEntry(TCache &cache, const K &key) {
  return getCacheEntry(cache, key, [](const K &key) { return std::make_shared<TElem>(); });
}

// Groups drawables into pipeline
struct DrawableGrouper {
private:
  // Cache variable
  std::vector<const gfx::Feature *> features;
  const RenderTargetLayout &renderTargetLayout;

public:
  DrawableGrouper(const RenderTargetLayout &renderTargetLayout) : renderTargetLayout(renderTargetLayout) {}

  // Generate hash for the part of the render step that is shared across all drawables
  Hash128 generateSharedHash() {
    HasherXXH128<PipelineHashVisitor> sharedHasher;
    sharedHasher(renderTargetLayout);
    return sharedHasher.getDigest();
  }

  struct GroupedDrawable {
    Hash128 pipelineHash;
    CachedPipelinePtr cachedPipeline;
  };

  GroupedDrawable groupByPipeline(PipelineCache &pipelineCache, Hash128 sharedHash, const std::vector<FeaturePtr> &baseFeatures,
                                  const std::vector<const MaterialParameters *> &baseParameters, const IDrawable &drawable) {

    // TODO: Processor
    // assert(drawable.mesh);
    // const Mesh &mesh = *drawable.mesh.get();
    // const Material *material = drawable.material.get();

    features.clear();
    auto collectFeatures = [&](const std::vector<FeaturePtr> &inFeatures) {
      for (auto &feature : inFeatures) {
        features.push_back(feature.get());
      }
    };

    // TODO: Processor
    // Collect features from various sources
    // collectFeatures(baseFeatures);
    // collectFeatures(drawable.features);
    // if (material)
    //   collectFeatures(material->features);

    HasherXXH128 featureHasher;
    for (auto &feature : features) {
      featureHasher(feature->id);
    }
    Hash128 featureHash = featureHasher.getDigest();

    HasherXXH128<PipelineHashVisitor> hasher;
    hasher(sharedHash);
    // hasher(mesh.getFormat());
    hasher(featureHash);

    // Collect material parameters from various sources
    for (const MaterialParameters *baseParam : baseParameters) {
      assert(baseParam);
      hasher(*baseParam);
    }

    // TODO: Processor
    // if (material) {
    //   hasher(*material);
    // }

    // TODO: processor
    // hasher(drawable.parameters);

    Hash128 pipelineHash = hasher.getDigest();

    auto &cachedPipeline = getCacheEntry(pipelineCache.map, pipelineHash, [&](const Hash128 &key) {
      auto result = std::make_shared<CachedPipeline>();
      auto &cachedPipeline = *result.get();

      // TODO: Processor
      // cachedPipeline.meshFormat = mesh.getFormat();
      cachedPipeline.features = features;
      cachedPipeline.renderTargetLayout = renderTargetLayout;

      auto collectMaterialParameters = [&](const MaterialParameters &inMaterialParameters) {
        for (auto &pair : inMaterialParameters.texture)
          cachedPipeline.materialTextureBindings.insert(pair);
      };

      // Collect material parameters from various sources
      for (const MaterialParameters *baseParam : baseParameters) {
        collectMaterialParameters(*baseParam);
      }

      // TODO: Processor
      // if (material)
      //   collectMaterialParameters(material->parameters);
      // collectMaterialParameters(drawable.parameters);

      return result;
    });

    return GroupedDrawable{
        .pipelineHash = pipelineHash,
        .cachedPipeline = cachedPipeline,
    };
  }

  void groupByPipeline(PipelineDrawableCache &pipelineDrawableCache, PipelineCache &pipelineCache, Hash128 sharedHash,
                       const std::vector<FeaturePtr> &baseFeatures, const std::vector<const MaterialParameters *> &baseParameters,
                       const std::vector<const IDrawable *> &drawables) {
    for (auto &drawable : drawables) {
      GroupedDrawable grouped = groupByPipeline(pipelineCache, sharedHash, baseFeatures, baseParameters, *drawable);

      // Insert the result into the pipelineDrawableCache
      auto &pipelineDrawables = getCacheEntry(pipelineDrawableCache.map, grouped.pipelineHash, [&](const Hash128 &key) {
        return PipelineDrawables{
            .cachedPipeline = grouped.cachedPipeline,
        };
      });

      pipelineDrawables.drawables.push_back(drawable);
    }
  }
};

template <typename T> void touchCacheItem(T &item, size_t frameCounter) { item.lastTouched = frameCounter; }
template <typename T> void touchCacheItemPtr(std::shared_ptr<T> &item, size_t frameCounter) {
  touchCacheItem(*item.get(), frameCounter);
}

} // namespace gfx::detail

#endif /* A2997EDF_4E03_48F4_AF46_10E0BEE1DFE3 */
