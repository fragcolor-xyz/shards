#ifndef A569D4E8_ACA8_49B1_9D91_A7936E698D61
#define A569D4E8_ACA8_49B1_9D91_A7936E698D61

#include "fwd.hpp"
#include "enums.hpp"
#include "render_target.hpp"
#include "wgpu_handle.hpp"
#include "renderer_cache.hpp"
#include "texture.hpp"
#include "gfx_wgpu.hpp"
#include <compare>
#include <shared_mutex>
#include <unordered_map>
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
  UniqueId id;
  size_t version;
  TextureViewDesc desc;

  std::strong_ordering operator<=>(const TextureViewKey &) const = default;
  friend size_t hash_value(TextureViewKey const &v) {
    size_t result{};
    boost::hash_combine(result, size_t(v.id));
    boost::hash_combine(result, size_t(v.version));
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

  WGPUTextureView getTextureView(size_t frameCounter, TexturePtr texture, WGPUTexture wgpuTexture, TextureViewDesc desc) {
    return getTextureView(frameCounter, TextureViewKey{texture->getId(), texture->getVersion(), desc}, wgpuTexture);
  }

  WGPUTextureView getTextureView(size_t frameCounter, TextureViewKey key, WGPUTexture texture) {
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
      WGPUTextureView view = wgpuTextureCreateView(texture, &viewDesc);
      shassert(view);

      mutex.lock();
      it = cache.emplace(key, view).first;
      mutex.unlock();
    } else {
      mutex.unlock_shared();
    }
    it->second.touch(frameCounter);
    return it->second.view;
  }

  WGPUTextureView getTextureView(size_t frameCounter, const TextureContextData &textureData, TextureViewDesc desc) {
    WGPUTexture texture = textureData.externalTexture ? textureData.externalTexture : textureData.texture;
    return getTextureView(frameCounter, TextureViewKey{textureData.id, textureData.version, desc}, texture);
  }

  WGPUTextureView getDefaultTextureView(size_t frameCounter, const TextureContextData &textureData) {
    TextureViewDesc defaultDesc{
        .format = textureData.format.pixelFormat,
        .baseMipLevel = 0,
        .mipLevelCount = textureData.format.mipLevels,
        .baseArrayLayer = 0,
        .arrayLayerCount = 1,
        .aspect = WGPUTextureAspect_All,
    };

    switch (textureData.format.dimension) {
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
    return getTextureView(frameCounter, textureData, defaultDesc);
  }
};
} // namespace gfx

#endif /* A569D4E8_ACA8_49B1_9D91_A7936E698D61 */
