#pragma once
#include "context.hpp"
#include "gfx_wgpu.hpp"
#include <vector>

namespace gfx {

// Texture filled with a single color used as placeholder for unbound texture slots
struct PlaceholderTexture {
	WGPUTexture texture;
	WGPUTextureView textureView;
	WGPUSampler sampler;

	PlaceholderTexture() = default;
	PlaceholderTexture(const PlaceholderTexture &other) = delete;
	PlaceholderTexture(Context &context, int2 resolution, float4 fillColor) {
		std::vector<uint32_t> pixelData;
		size_t numPixels = resolution.x * resolution.y;
		pixelData.resize(numPixels);
		for (size_t p = 0; p < numPixels; p++) {
			Color c = colorFromFloat(fillColor);
			uint32_t *pixel = (uint32_t *)(pixelData.data() + p);
			memcpy(pixel, &c.x, sizeof(uint32_t));
		}

		WGPUTextureDescriptor desc{};
		desc.dimension = WGPUTextureDimension_2D;
		desc.format = WGPUTextureFormat_RGBA8UnormSrgb;
		desc.mipLevelCount = 1;
		desc.sampleCount = 1;
		desc.size.width = resolution.x;
		desc.size.height = resolution.y;
		desc.size.depthOrArrayLayers = 1;
		desc.usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst;
		texture = wgpuDeviceCreateTexture(context.wgpuDevice, &desc);

		WGPUImageCopyTexture copyTextureDst{};
		copyTextureDst.texture = texture;
		copyTextureDst.aspect = WGPUTextureAspect_All;
		copyTextureDst.mipLevel = 0;
		WGPUTextureDataLayout layout{};
		layout.bytesPerRow = sizeof(uint32_t) * resolution.x;
		wgpuQueueWriteTexture(context.wgpuQueue, &copyTextureDst, pixelData.data(), pixelData.size() * sizeof(uint32_t), &layout, &desc.size);

		WGPUTextureViewDescriptor viewDesc{};
		viewDesc.baseArrayLayer = 0;
		viewDesc.arrayLayerCount = 1;
		viewDesc.baseMipLevel = 0;
		viewDesc.mipLevelCount = 1;
		viewDesc.aspect = WGPUTextureAspect_All;
		viewDesc.dimension = WGPUTextureViewDimension_2D;
		viewDesc.format = desc.format;
		textureView = wgpuTextureCreateView(texture, &viewDesc);

		WGPUSamplerDescriptor samplerDesc{};
		samplerDesc.addressModeU = WGPUAddressMode_Repeat;
		samplerDesc.addressModeV = WGPUAddressMode_Repeat;
		samplerDesc.addressModeW = WGPUAddressMode_Repeat;
		samplerDesc.lodMinClamp = 0;
		samplerDesc.lodMaxClamp = 0;
		samplerDesc.magFilter = WGPUFilterMode_Nearest;
		samplerDesc.minFilter = WGPUFilterMode_Nearest;
		samplerDesc.mipmapFilter = WGPUFilterMode_Nearest;
		samplerDesc.maxAnisotropy = 0;
		sampler = wgpuDeviceCreateSampler(context.wgpuDevice, &samplerDesc);
	}

	~PlaceholderTexture() {
		WGPU_SAFE_RELEASE(wgpuTextureRelease, texture);
		WGPU_SAFE_RELEASE(wgpuTextureViewRelease, textureView);
		WGPU_SAFE_RELEASE(wgpuSamplerRelease, sampler);
	}
};
} // namespace gfx
