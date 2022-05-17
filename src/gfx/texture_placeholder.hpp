#ifndef GFX_TEXTURE_PLACEHOLDER
#define GFX_TEXTURE_PLACEHOLDER

#include "context.hpp"
#include "context_data.hpp"
#include "gfx_wgpu.hpp"
#include <vector>

namespace gfx {

// Texture filled with a single color used as placeholder for unbound texture slots
struct PlaceholderTextureContextData : public ContextData {
  WGPUTexture texture;
  WGPUTextureView textureView;
  WGPUSampler sampler;

  ~PlaceholderTextureContextData() { releaseContextDataConditional(); }
  void releaseContextData() override {
    WGPU_SAFE_RELEASE(wgpuTextureRelease, texture);
    WGPU_SAFE_RELEASE(wgpuTextureViewRelease, textureView);
    WGPU_SAFE_RELEASE(wgpuSamplerRelease, sampler);
  }
};

struct PlaceholderTexture final : public TWithContextData<PlaceholderTextureContextData> {
  int2 resolution = int2(2, 2);
  float4 fillColor = float4(0, 0, 0, 0);

  PlaceholderTexture() = default;
  PlaceholderTexture(const PlaceholderTexture &other) = delete;
  PlaceholderTexture(int2 resolution, float4 fillColor) : resolution(resolution), fillColor(fillColor) {}

protected:
  void initContextData(Context &context, PlaceholderTextureContextData &data) {
    std::vector<uint32_t> pixelData;
    size_t numPixels = resolution.x * resolution.y;
    pixelData.resize(numPixels);
    for (size_t p = 0; p < numPixels; p++) {
      Color c = colorFromFloat(fillColor);
      uint32_t *pixel = (uint32_t *)(pixelData.data() + p);
      memcpy(pixel, &c.x, sizeof(uint32_t));
    }

    WGPUTextureDescriptor desc{};
    desc.label = "placeholder";
    desc.dimension = WGPUTextureDimension_2D;
    desc.format = WGPUTextureFormat_RGBA8UnormSrgb;
    desc.mipLevelCount = 1;
    desc.sampleCount = 1;
    desc.size.width = resolution.x;
    desc.size.height = resolution.y;
    desc.size.depthOrArrayLayers = 1;
    desc.usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst;
    data.texture = wgpuDeviceCreateTexture(context.wgpuDevice, &desc);

    WGPUImageCopyTexture copyTextureDst{};
    copyTextureDst.texture = data.texture;
    copyTextureDst.aspect = WGPUTextureAspect_All;
    copyTextureDst.mipLevel = 0;
    WGPUTextureDataLayout layout{};
    layout.bytesPerRow = sizeof(uint32_t) * resolution.x;
    layout.rowsPerImage = uint32_t(resolution.y);
    wgpuQueueWriteTexture(context.wgpuQueue, &copyTextureDst, pixelData.data(), pixelData.size() * sizeof(uint32_t), &layout,
                          &desc.size);

    WGPUTextureViewDescriptor viewDesc{};
    viewDesc.baseArrayLayer = 0;
    viewDesc.arrayLayerCount = 1;
    viewDesc.baseMipLevel = 0;
    viewDesc.mipLevelCount = 1;
    viewDesc.aspect = WGPUTextureAspect_All;
    viewDesc.dimension = WGPUTextureViewDimension_2D;
    viewDesc.format = desc.format;
    data.textureView = wgpuTextureCreateView(data.texture, &viewDesc);

    WGPUSamplerDescriptor samplerDesc{};
    samplerDesc.addressModeU = WGPUAddressMode_Repeat;
    samplerDesc.addressModeV = WGPUAddressMode_Repeat;
    samplerDesc.addressModeW = WGPUAddressMode_Repeat;
    samplerDesc.lodMinClamp = 0;
    samplerDesc.lodMaxClamp = 0;
    samplerDesc.magFilter = WGPUFilterMode_Nearest;
    samplerDesc.minFilter = WGPUFilterMode_Nearest;
    samplerDesc.mipmapFilter = WGPUMipmapFilterMode_Nearest;
    samplerDesc.maxAnisotropy = 0;
    data.sampler = wgpuDeviceCreateSampler(context.wgpuDevice, &samplerDesc);
  }
};
} // namespace gfx

#endif // GFX_TEXTURE_PLACEHOLDER
