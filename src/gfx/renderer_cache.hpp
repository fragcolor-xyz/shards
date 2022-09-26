#ifndef A2997EDF_4E03_48F4_AF46_10E0BEE1DFE3
#define A2997EDF_4E03_48F4_AF46_10E0BEE1DFE3

#include "renderer_types.hpp"
#include "hasherxxh128.hpp"
#include <unordered_map>

namespace gfx::detail {
struct PipelineCache {
  std::unordered_map<Hash128, CachedPipelinePtr> map;

  void clear() { map.clear(); }
};

// Associates drawables with the render pipeline they belong to
// TODO: Find a better name
struct PipelineDrawableCache {
  std::unordered_map<Hash128, PipelineDrawables> map;
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
  std::vector<const Feature *> features;
  RenderTargetLayout &renderTargetLayout;

public:
  DrawableGrouper(RenderTargetLayout &renderTargetLayout) : renderTargetLayout(renderTargetLayout) {}

  // Generate hash for the part of the render step that is shared across all drawables
  Hash128 generateSharedHash(const RenderTargetLayout &renderTargetLayout) {
    HasherXXH128<HashStaticVistor> sharedHasher;
    sharedHasher(renderTargetLayout);
    return sharedHasher.getDigest();
  }

  struct GroupedDrawable {
    Hash128 pipelineHash;
    CachedPipelinePtr cachedPipeline;
  };

  GroupedDrawable groupByPipeline(PipelineCache &pipelineCache, Hash128 sharedHash, const std::vector<FeaturePtr> &baseFeatures,
                                  const std::vector<const MaterialParameters *> &baseParameters, const Drawable &drawable) {

    assert(drawable.mesh);
    const Mesh &mesh = *drawable.mesh.get();
    const Material *material = drawable.material.get();

    features.clear();
    auto collectFeatures = [&](const std::vector<FeaturePtr> &inFeatures) {
      for (auto &feature : inFeatures) {
        features.push_back(feature.get());
      }
    };

    // Collect features from various sources
    collectFeatures(baseFeatures);
    collectFeatures(drawable.features);
    if (material)
      collectFeatures(material->features);

    HasherXXH128 featureHasher;
    for (auto &feature : features) {
      // NOTE: Hashed by pointer since features are considered immutable & shared/ref-counted
      featureHasher(feature);
    }
    Hash128 featureHash = featureHasher.getDigest();

    HasherXXH128<HashStaticVistor> hasher;
    hasher(sharedHash);
    hasher(mesh.getFormat());
    hasher(featureHash);

    // Collect material parameters from various sources
    for (const MaterialParameters *baseParam : baseParameters) {
      assert(baseParam);
      hasher(*baseParam);
    }
    if (material) {
      hasher(*material);
    }
    hasher(drawable.parameters);

    Hash128 pipelineHash = hasher.getDigest();

    auto &cachedPipeline = getCacheEntry(pipelineCache.map, pipelineHash, [&](const Hash128 &key) {
      auto result = std::make_shared<CachedPipeline>();
      auto &cachedPipeline = *result.get();

      cachedPipeline.meshFormat = mesh.getFormat();
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
      if (material)
        collectMaterialParameters(material->parameters);
      collectMaterialParameters(drawable.parameters);

      return result;
    });

    return GroupedDrawable{
        .pipelineHash = pipelineHash,
        .cachedPipeline = cachedPipeline,
    };
  }

  void groupByPipeline(PipelineDrawableCache &pipelineDrawableCache, PipelineCache &pipelineCache, Hash128 sharedHash,
                       const std::vector<FeaturePtr> &baseFeatures, const std::vector<const MaterialParameters *> &baseParameters,
                       const std::vector<DrawablePtr> &drawables) {
    for (auto &drawable : drawables) {
      GroupedDrawable grouped = groupByPipeline(pipelineCache, sharedHash, baseFeatures, baseParameters, *drawable.get());

      // Insert the result into the pipelineDrawableCache
      auto &pipelineDrawables = getCacheEntry(pipelineDrawableCache.map, grouped.pipelineHash, [&](const Hash128 &key) {
        return PipelineDrawables {
          .cachedPipeline = grouped.cachedPipeline,
        };
      });

      pipelineDrawables.drawables.push_back(drawable.get());
    }
  }
};

} // namespace gfx::detail

#endif /* A2997EDF_4E03_48F4_AF46_10E0BEE1DFE3 */
