#ifndef FC87BDC3_5D05_40D2_8931_886FBEB83E6D
#define FC87BDC3_5D05_40D2_8931_886FBEB83E6D

#include "fwd.hpp"
#include "enums.hpp"
#include "wgpu_handle.hpp"
#include "texture.hpp"
#include "gfx_wgpu.hpp"
#include "renderer_cache.hpp"
#include <compare>
#include <shared_mutex>
#include <unordered_map>
#include <boost/container_hash/hash.hpp>

namespace gfx {
struct SamplerKey {
  uint8_t numMipLevels;
  bool allowFiltering{};
  SamplerState desc;

  std::strong_ordering operator<=>(const SamplerKey &) const = default;
  friend size_t hash_value(SamplerKey const &v) {
    size_t result{};
    boost::hash_combine(result, v.numMipLevels);
    boost::hash_combine(result, v.desc);
    return result;
  }
};

struct SamplerCache {
  struct Entry {
    WgpuHandle<WGPUSampler> sampler;
    size_t lastTouched{};

    Entry(WGPUSampler sampler) : sampler(sampler) {}
    void touch(size_t frameCounter) { lastTouched = frameCounter; }
  };

  WGPUDevice wgpuDevice;
  std::unordered_map<SamplerKey, Entry, boost::hash<SamplerKey>> cache;
  std::shared_mutex mutex;

  SamplerCache(WGPUDevice wgpuDevice) : wgpuDevice(wgpuDevice) {}

  void clearOldCacheItems(size_t frameCounter, size_t frameThreshold) {
    mutex.lock();
    detail::clearOldCacheItemsIn(cache, frameCounter, frameThreshold);
    mutex.unlock();
  }

  WGPUSampler getSampler(size_t frameCounter, uint8_t numMipLevels, bool allowFiltering, SamplerState desc) {
    return getSampler(frameCounter, SamplerKey{numMipLevels, allowFiltering, desc});
  }

  WGPUSampler getSampler(size_t frameCounter, SamplerKey key) {
    mutex.lock_shared();
    auto it = cache.find(key);
    if (it == cache.end()) {
      mutex.unlock_shared();

      WGPUSamplerDescriptor desc{};
      desc.addressModeU = key.desc.addressModeU;
      desc.addressModeV = key.desc.addressModeV;
      desc.addressModeW = key.desc.addressModeW;
      desc.lodMinClamp = 0;
      desc.lodMaxClamp = key.numMipLevels;
      if (key.allowFiltering) {
        desc.magFilter = key.desc.filterMode;
        desc.minFilter = key.desc.filterMode;
        desc.mipmapFilter = (key.numMipLevels > 1) ? WGPUMipmapFilterMode_Linear : WGPUMipmapFilterMode_Nearest;
      } else {
        desc.magFilter = WGPUFilterMode_Nearest;
        desc.minFilter = WGPUFilterMode_Nearest;
        desc.mipmapFilter = WGPUMipmapFilterMode_Nearest;
      }
      desc.maxAnisotropy = 1;

      WGPUSampler sampler = wgpuDeviceCreateSampler(wgpuDevice, &desc);
      shassert(sampler);

      mutex.lock();
      it = cache.emplace(key, sampler).first;
      mutex.unlock();
    } else {
      mutex.unlock_shared();
    }
    it->second.touch(frameCounter);
    return it->second.sampler;
  }

  WGPUSampler getDefaultSampler(size_t frameCounter, bool allowFiltering, const TexturePtr &texture) {
    return getSampler(frameCounter, texture->getFormat().mipLevels, allowFiltering, texture->getSamplerState());
  }
};
} // namespace gfx
#endif /* FC87BDC3_5D05_40D2_8931_886FBEB83E6D */
