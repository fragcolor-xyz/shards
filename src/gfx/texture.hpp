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
	WGPUExtent3D size{};

	~TextureContextData() { releaseConditional(); }
	void release() {
		WGPU_SAFE_RELEASE(wgpuTextureRelease, texture);
		WGPU_SAFE_RELEASE(wgpuTextureViewRelease, defaultView);
	}
};

struct Texture final : public TWithContextData<TextureContextData> {
private:
	TextureFormat format;
	ImmutableSharedBuffer data;
	int2 resolution{};

public:
	const TextureFormat &getFormat() const { return format; }

	// Creates a blank texture
	void init(const TextureFormat &format, int2 resolution);

	// Creates or updates a texture with image data
	void update(const TextureFormat &format, int2 resolution, const ImmutableSharedBuffer &data, size_t faceIndex = 0);

protected:
	void initContextData(Context &context, TextureContextData &contextData);
};

} // namespace gfx
