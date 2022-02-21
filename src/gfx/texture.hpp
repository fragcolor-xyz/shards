#pragma once
#include "context_data.hpp"
#include "gfx_wgpu.hpp"
#include "isb.hpp"
#include "linalg.hpp"
#include <variant>

namespace gfx {

enum class TextureFormatFlags : uint8_t {
	AutoGenerateMips = 0x01,
};
inline bool TextureFormatFlagsContains(TextureFormatFlags left, TextureFormatFlags right) { return ((uint8_t &)left & (uint8_t &)right) != 0; }

enum class TextureType : uint8_t {
	D1,
	D2,
	Cube,
};

struct SamplerState {
	WGPUAddressMode addressModeU = WGPUAddressMode::WGPUAddressMode_Repeat;
	WGPUAddressMode addressModeV = WGPUAddressMode::WGPUAddressMode_Repeat;
	WGPUAddressMode addressModeW = WGPUAddressMode::WGPUAddressMode_Repeat;
	WGPUFilterMode filterMode = WGPUFilterMode::WGPUFilterMode_Linear;

	template <typename T> void hashStatic(T &hasher) const {
		hasher(addressModeU);
		hasher(addressModeV);
		hasher(addressModeW);
		hasher(filterMode);
	}
};

struct TextureFormat {
	TextureType type = TextureType::D2;
	TextureFormatFlags flags = TextureFormatFlags::AutoGenerateMips;
	WGPUTextureFormat pixelFormat = WGPUTextureFormat::WGPUTextureFormat_Undefined;

	bool hasMips() { return TextureFormatFlagsContains(flags, TextureFormatFlags::AutoGenerateMips); }
};

struct TextureContextData : public ContextData {
	TextureFormat format;
	WGPUTexture texture = nullptr;
	WGPUTextureView defaultView = nullptr;
	WGPUSampler sampler;

	WGPUExtent3D size{};

	~TextureContextData() { releaseConditional(); }
	void release() {
		WGPU_SAFE_RELEASE(wgpuTextureRelease, texture);
		WGPU_SAFE_RELEASE(wgpuTextureViewRelease, defaultView);
		WGPU_SAFE_RELEASE(wgpuSamplerRelease, sampler);
	}
};

struct Texture final : public TWithContextData<TextureContextData> {
private:
	TextureFormat format;
	ImmutableSharedBuffer data;
	SamplerState samplerState;
	int2 resolution{};

public:
	const TextureFormat &getFormat() const { return format; }

	// Creates a texture
	void init(const TextureFormat &format, int2 resolution, const SamplerState &samplerState = SamplerState(),
			  const ImmutableSharedBuffer &data = ImmutableSharedBuffer());

	void setSamplerState(const SamplerState &samplerState);

	std::shared_ptr<Texture> clone();

protected:
	void initContextData(Context &context, TextureContextData &contextData);
	void updateContextData(Context &context, TextureContextData &contextData);
};

} // namespace gfx
