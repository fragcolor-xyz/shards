#ifndef A2997EDF_4E03_48F4_AF46_10E0BEE1DFE3
#define A2997EDF_4E03_48F4_AF46_10E0BEE1DFE3

#include "renderer_types.hpp"
#include "hasherxxh128.hpp"
#include <magic_enum.hpp>
#include <unordered_map>

namespace gfx {

struct References {
  std::set<UniqueId> set;

  void add(UniqueId id) { set.insert(id); }
  template <typename T> void add(const std::shared_ptr<T> &ref) { add(ref->getId()); }

  bool contains(UniqueId id) const { return set.contains(id); }
  template <typename T> bool contains(const std::shared_ptr<T> &ref) const { return contains(ref->getId()); }
};

// Does 2 things while traversing gfx objects:
// - Collect the static hash to determine pipeline permutation
// - Collect references to other gfx objects that the permutation depends on
struct HashCollector {
  References references;
  HasherXXH128<HashStaticVistor> hasher;

  void addReference(UniqueId id) { references.add(id); }
  template <typename T> void addReference(const std::shared_ptr<T> &ref) { references.add(ref->getId()); }
  template <typename T> void operator()(const T &val) { hasher(val); }
};
} // namespace gfx

namespace gfx::detail {
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

struct RenderCache {
  struct Entry {
    References references;
  };
  std::map<UniqueId, Entry> perType[magic_enum::enum_count<UniqueIdTag>()];

  void evict(UniqueId id) {}
  // Functions to compute the base hash for each type
  // Hash128 hash(MeshPtr mesh) {}
  // Hash128 hash(FeaturePtr mesh) {}
  // Hash128 hash(MaterialPtr mesh) {}
  // Hash128 hash(DrawablePtr mesh) {}
  // Hash128 hash(DrawablePtr mesh, std::vector<const gfx::Feature *> features) {}
};

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
    HasherXXH128<HashStaticVistor> sharedHasher;
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

    HasherXXH128<HashStaticVistor> hasher;
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
