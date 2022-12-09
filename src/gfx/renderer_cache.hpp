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
