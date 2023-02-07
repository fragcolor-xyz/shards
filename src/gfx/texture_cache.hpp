#ifndef A569D4E8_ACA8_49B1_9D91_A7936E698D61
#define A569D4E8_ACA8_49B1_9D91_A7936E698D61

#include "boost/container_hash/hash_fwd.hpp"
#include "fwd.hpp"
#include "gfx/enums.hpp"
#include "gfx/render_target.hpp"
#include "gfx/wgpu_handle.hpp"
#include "gfx/renderer_cache.hpp"
#include "texture.hpp"
#include <compare>
#include <shared_mutex>
#include <unordered_map>
#include <webgpu-headers/webgpu.h>
#include <boost/container_hash/hash.hpp>

namespace gfx {
struct TextureViewDesc {
  WGPUTextureFormat format;
  WGPUTextureViewDimension dimension;
  uint32_t baseMipLevel;
  uint32_t mipLevelCount;
  uint32_t baseArrayLayer;
  uint32_t arrayLayerCount;
  WGPUTextureAspect aspect;

  std::strong_ordering operator<=>(const TextureViewDesc &) const = default;
  friend size_t hash_value(TextureViewDesc const &v) {
    size_t result{};
    boost::hash_combine(result, uint32_t(v.format));
    boost::hash_combine(result, uint32_t(v.dimension));
    boost::hash_combine(result, v.baseMipLevel);
    boost::hash_combine(result, v.mipLevelCount);
    boost::hash_combine(result, v.baseArrayLayer);
    boost::hash_combine(result, v.arrayLayerCount);
    boost::hash_combine(result, uint32_t(v.aspect));
    return result;
  }
};

struct TextureViewKey {
  WGPUTexture texture;
  TextureViewDesc desc;

  std::strong_ordering operator<=>(const TextureViewKey &) const = default;
  friend size_t hash_value(TextureViewKey const &v) {
    size_t result{};
    boost::hash_combine(result, size_t(v.texture));
    boost::hash_combine(result, v.desc);
    return result;
  }
};

struct TextureViewCache {

  struct Entry {
    WgpuHandle<WGPUTextureView> view;
    size_t lastTouched{};

    Entry(WGPUTextureView view) : view(view) {}
    void touch(size_t frameCounter) { lastTouched = frameCounter; }
  };

  std::unordered_map<TextureViewKey, Entry, boost::hash<TextureViewKey>> cache;
  std::shared_mutex mutex;

  void clearOldCacheItems(size_t frameCounter, size_t frameThreshold) {
    mutex.lock();
    detail::clearOldCacheItemsIn(cache, frameCounter, frameThreshold);
    mutex.unlock();
  }

  WGPUTextureView getTextureView(size_t frameCounter, WGPUTexture texture, TextureViewDesc desc) {
    return getTextureView(frameCounter, TextureViewKey{texture, desc});
  }

  WGPUTextureView getTextureView(size_t frameCounter, TextureViewKey key) {
    mutex.lock_shared();
    auto it = cache.find(key);
    if (it == cache.end()) {
      mutex.unlock_shared();

      WGPUTextureViewDescriptor viewDesc{
          .format = key.desc.format,
          .dimension = key.desc.dimension,
          .baseMipLevel = key.desc.baseMipLevel,
          .mipLevelCount = key.desc.mipLevelCount,
          .baseArrayLayer = key.desc.baseArrayLayer,
          .arrayLayerCount = key.desc.arrayLayerCount,
          .aspect = key.desc.aspect,
      };
      WGPUTextureView view = wgpuTextureCreateView(key.texture, &viewDesc);
      assert(view);

      mutex.lock();
      it = cache.emplace(key, view).first;
      mutex.unlock();
    } else {
      mutex.unlock_shared();
    }
    it->second.touch(frameCounter);
    return it->second.view;
  }

  WGPUTextureView getDefaultTextureView(size_t frameCounter, const TextureContextData &textureData) {
    TextureViewDesc defaultDesc{
        .format = textureData.format.pixelFormat,
        .baseMipLevel = 0,
        .mipLevelCount = 1,
        .baseArrayLayer = 0,
        .arrayLayerCount = 1,
        .aspect = WGPUTextureAspect_All,
    };

    switch (textureData.format.type) {
    case TextureDimension::D1:
      defaultDesc.dimension = WGPUTextureViewDimension_1D;
      break;
    case TextureDimension::D2:
      defaultDesc.dimension = WGPUTextureViewDimension_2D;
      break;
    case TextureDimension::Cube:
      defaultDesc.dimension = WGPUTextureViewDimension_Cube;
      defaultDesc.arrayLayerCount = 6;
      break;
    }
    return getTextureView(frameCounter, textureData.texture, defaultDesc);
  }
};
} // namespace gfx

#endif /* A569D4E8_ACA8_49B1_9D91_A7936E698D61 */
