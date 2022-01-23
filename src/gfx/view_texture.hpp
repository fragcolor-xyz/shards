#pragma once
#include "gfx_wgpu.hpp"
#include "linalg.hpp"

namespace gfx {

struct ViewTexture {
private:
  WGPUTexture texture{};
  WGPUTextureView textureView{};
  int2 size{};
  WGPUTextureFormat format{};

public:
  ViewTexture(WGPUTextureFormat format) : format(format) {}
  ~ViewTexture() { cleanup(); }

  ViewTexture(const ViewTexture &other) = delete;
  ViewTexture &operator=(const ViewTexture &other) = delete;
  WGPUTextureFormat getFormat() const { return format; }

  void cleanup() {
    WGPU_SAFE_RELEASE(wgpuTextureViewRelease, textureView);
    WGPU_SAFE_RELEASE(wgpuTextureRelease, texture);
  }

  WGPUTextureView update(WGPUDevice device, int2 size) {
    if (!texture || !textureView || size != this->size) {
      cleanup();
      this->size = size;

      WGPUTextureDescriptor textureDesc{};
      textureDesc.dimension = WGPUTextureDimension_2D;
      textureDesc.format = format;
      textureDesc.sampleCount = 1;
      textureDesc.mipLevelCount = 1;
      textureDesc.usage = WGPUTextureUsage_RenderAttachment;
      textureDesc.size.depthOrArrayLayers = 1;
      textureDesc.size.width = size.x;
      textureDesc.size.height = size.y;
      texture = wgpuDeviceCreateTexture(device, &textureDesc);

      WGPUTextureViewDescriptor viewDesc{};
      viewDesc.arrayLayerCount = 1;
      viewDesc.baseArrayLayer = 0;
      viewDesc.mipLevelCount = 1;
      viewDesc.baseMipLevel = 0;
      viewDesc.dimension = WGPUTextureViewDimension_2D;
      viewDesc.aspect = WGPUTextureAspect_All;
      viewDesc.format = format;
      textureView = wgpuTextureCreateView(texture, &viewDesc);
    }

    return textureView;
  }
};

} // namespace gfx