#include "texture.hpp"
#include "context.hpp"
#include "error_utils.hpp"
#include <magic_enum.hpp>
#include <map>

namespace gfx {

struct InputTextureFormat {
	uint8_t pixelSize;
};

typedef std::map<WGPUTextureFormat, InputTextureFormat> InputTextureFormatMap;
static const InputTextureFormatMap &getInputTextureFormatMap() {
	static InputTextureFormatMap instance = []() {
		return InputTextureFormatMap{
			{WGPUTextureFormat::WGPUTextureFormat_R8Unorm, {1}},
			// u8x1
			{WGPUTextureFormat_R8Snorm, {1}},
			{WGPUTextureFormat_R8Uint, {1}},
			{WGPUTextureFormat_R8Sint, {1}},
			// u8x2
			{WGPUTextureFormat_RG8Unorm, {2}},
			{WGPUTextureFormat_RG8Snorm, {2}},
			{WGPUTextureFormat_RG8Uint, {2}},
			{WGPUTextureFormat_RG8Sint, {2}},
			// u8x4
			{WGPUTextureFormat_RGBA8Unorm, {4}},
			{WGPUTextureFormat_RGBA8UnormSrgb, {4}},
			{WGPUTextureFormat_RGBA8Snorm, {4}},
			{WGPUTextureFormat_RGBA8Uint, {4}},
			{WGPUTextureFormat_RGBA8Sint, {4}},
			{WGPUTextureFormat_BGRA8Unorm, {4}},
			{WGPUTextureFormat_BGRA8UnormSrgb, {4}},
			// u16x1
			{WGPUTextureFormat_R16Uint, {2}},
			{WGPUTextureFormat_R16Sint, {2}},
			{WGPUTextureFormat_R16Float, {2}},
			// u16x2
			{WGPUTextureFormat_RG16Uint, {4}},
			{WGPUTextureFormat_RG16Sint, {4}},
			{WGPUTextureFormat_RG16Float, {4}},
			// u16x4
			{WGPUTextureFormat_RGBA16Uint, {8}},
			{WGPUTextureFormat_RGBA16Sint, {8}},
			{WGPUTextureFormat_RGBA16Float, {8}},
			// u32x1
			{WGPUTextureFormat_R32Float, {4}},
			{WGPUTextureFormat_R32Uint, {4}},
			{WGPUTextureFormat_R32Sint, {4}},
			// u32x2
			{WGPUTextureFormat_RG32Float, {8}},
			{WGPUTextureFormat_RG32Uint, {8}},
			{WGPUTextureFormat_RG32Sint, {8}},
			// u32x4
			{WGPUTextureFormat_RGBA32Float, {16}},
			{WGPUTextureFormat_RGBA32Uint, {16}},
			{WGPUTextureFormat_RGBA32Sint, {16}},
		};
	}();
	return instance;
}

void Texture::init(const TextureFormat &format, int2 resolution) {}

void Texture::update(const TextureFormat &format, int2 resolution, const ImmutableSharedBuffer &data, size_t faceIndex) {
	contextData.reset();
	this->format = format;
	this->resolution = resolution;
	this->data = data;
}

void Texture::initContextData(Context &context, TextureContextData &contextData) {
	WGPUDevice device = context.wgpuDevice;
	assert(device);

	contextData.size.width = resolution.x;
	contextData.size.height = resolution.y;
	contextData.size.depthOrArrayLayers = 1;
	contextData.format = format;

	WGPUTextureDescriptor desc = {};
	desc.usage = WGPUTextureUsage_TextureBinding;
	if (data) {
		desc.usage |= WGPUTextureUsage_CopyDst;
	}

	switch (format.type) {
	case TextureType::D1:
		desc.dimension = WGPUTextureDimension_1D;
		contextData.size.height = 1;
		break;
	case TextureType::D2:
		desc.dimension = WGPUTextureDimension_2D;
		break;
	case TextureType::Cube:
		desc.dimension = WGPUTextureDimension_2D;
		contextData.size.depthOrArrayLayers = 8;
		break;
	default:
		assert(false);
	}

	desc.size = contextData.size;
	desc.format = format.pixelFormat;
	desc.sampleCount = 1;
	desc.mipLevelCount = 1;

	contextData.texture = wgpuDeviceCreateTexture(context.wgpuDevice, &desc);

	// Optionally upload data
	if (data) {
		auto &inputTextureFormatMap = getInputTextureFormatMap();
		auto it = inputTextureFormatMap.find(format.pixelFormat);
		if (it == inputTextureFormatMap.end()) {
			throw formatException("Unsupported pixel format for texture initialization", magic_enum::enum_name(format.pixelFormat));
		}
		const InputTextureFormat &inputFormat = it->second;
		uint32_t rowDataLength = inputFormat.pixelSize * resolution.x;

		WGPUImageCopyTexture dst{
			.texture = contextData.texture,
			.mipLevel = 0,
			.aspect = WGPUTextureAspect_All,
		};
		WGPUTextureDataLayout layout{
			.bytesPerRow = rowDataLength,
		};
		WGPUExtent3D writeSize = contextData.size;
		wgpuQueueWriteTexture(context.wgpuQueue, &dst, data.getData(), data.getLength(), &layout, &writeSize);
	}

	// if (data) {
	// 	auto &inputTextureFormatMap = getInputTextureFormatMap();
	// 	auto it = inputTextureFormatMap.find(format.pixelFormat);
	// 	if (it == inputTextureFormatMap.end()) {
	// 		throw formatException("Unsupported pixel format for texture initialization", magic_enum::enum_name(format.pixelFormat));
	// 	}

	// 	const InputTextureFormat &inputFormat = it->second;
	// 	size_t rowDataLength = inputFormat.pixelSize * resolution.x;
	// 	size_t stagingBufferSize = rowDataLength * resolution.y;

	// 	WGPUBuffer stagingBuffer;
	// 	WGPUBufferDescriptor stagingBufferDesc;
	// 	stagingBufferDesc.mappedAtCreation = true;
	// 	stagingBufferDesc.size = stagingBufferSize;

	// 	if (data.getLength() != stagingBufferSize) {
	// 		throw formatException("Texture data length {} does not match expected length of {}", data.getLength(), stagingBufferSize);
	// 	}

	// 	stagingBuffer = wgpuDeviceCreateBuffer(context.wgpuDevice, &stagingBufferDesc);
	// 	void *mappedBufferData = wgpuBufferGetMappedRange(stagingBuffer, 0, stagingBufferSize);
	// 	memcpy(mappedBufferData, data.getData(), stagingBufferSize);
	// 	wgpuBufferUnmap(stagingBuffer);
	// }
}

} // namespace gfx
