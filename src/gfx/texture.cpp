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

void Texture::init(const TextureFormat &format, int2 resolution, const SamplerState &samplerState,
                   const ImmutableSharedBuffer &data) {
  contextData.reset();
  this->format = format;
  this->data = data;
  this->samplerState = samplerState;
  this->resolution = resolution;
}

void Texture::setSamplerState(const SamplerState &samplerState) {
  contextData.reset();
  this->samplerState = samplerState;
}

std::shared_ptr<Texture> Texture::clone() {
  std::shared_ptr<Texture> result = std::make_shared<Texture>(*this);
  result->contextData.reset();
  return result;
}

static WGPUSampler createSampler(Context &context, SamplerState samplerState, bool haveMips) {
  WGPUSamplerDescriptor desc{};
  desc.addressModeU = samplerState.addressModeU;
  desc.addressModeV = samplerState.addressModeV;
  desc.addressModeW = samplerState.addressModeW;
  desc.lodMinClamp = 0;
  desc.lodMaxClamp = 0;
  desc.magFilter = samplerState.filterMode;
  desc.minFilter = samplerState.filterMode;
  desc.mipmapFilter = haveMips ? WGPUMipmapFilterMode_Linear : WGPUMipmapFilterMode_Nearest;
  desc.maxAnisotropy = 0;
  return wgpuDeviceCreateSampler(context.wgpuDevice, &desc);
}

static WGPUTextureView createView(const TextureFormat &format, WGPUTexture texture) {
  WGPUTextureViewDescriptor viewDesc{};
  viewDesc.baseArrayLayer = 0;
  viewDesc.arrayLayerCount = 1;
  viewDesc.baseMipLevel = 0;
  viewDesc.mipLevelCount = 1;
  viewDesc.aspect = WGPUTextureAspect_All;
  viewDesc.dimension = WGPUTextureViewDimension_2D;
  viewDesc.format = format.pixelFormat;
  return wgpuTextureCreateView(texture, &viewDesc);
}

static void writeTextureData(Context &context, const TextureFormat &format, const int2 &resolution, WGPUTexture texture,
                             const ImmutableSharedBuffer &isb) {
  auto &inputTextureFormatMap = getInputTextureFormatMap();
  auto it = inputTextureFormatMap.find(format.pixelFormat);
  if (it == inputTextureFormatMap.end()) {
    throw formatException("Unsupported pixel format for texture initialization", magic_enum::enum_name(format.pixelFormat));
  }

  const InputTextureFormat &inputFormat = it->second;
  uint32_t rowDataLength = inputFormat.pixelSize * resolution.x;

  WGPUImageCopyTexture dst{
      .texture = texture,
      .mipLevel = 0,
      .aspect = WGPUTextureAspect_All,
  };
  WGPUTextureDataLayout layout{
      .bytesPerRow = rowDataLength,
      .rowsPerImage = uint32_t(resolution.y),
  };
  WGPUExtent3D writeSize{
      .width = uint32_t(resolution.x),
      .height = uint32_t(resolution.y),
      .depthOrArrayLayers = 1,
  };
  wgpuQueueWriteTexture(context.wgpuQueue, &dst, isb.getData(), isb.getLength(), &layout, &writeSize);
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

  if (data)
    writeTextureData(context, format, resolution, contextData.texture, data);

  contextData.defaultView = createView(format, contextData.texture);
  contextData.sampler = createSampler(context, samplerState, false);
}

void Texture::updateContextData(Context &context, TextureContextData &contextData) {}

} // namespace gfx
