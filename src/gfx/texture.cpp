#include "texture.hpp"
#include "context.hpp"
#include "error_utils.hpp"
#include <magic_enum.hpp>
#include <map>

namespace gfx {
TextureDesc TextureDesc::getDefault() {
  return TextureDesc{.format = {.pixelFormat = WGPUTextureFormat_RGBA8Unorm}, .resolution = int2(512, 512)};
}

std::shared_ptr<Texture> Texture::makeRenderAttachment(WGPUTextureFormat format, std::string &&label) {
  TextureDesc desc{
      .format =
          {
              .flags = TextureFormatFlags::RenderAttachment,
              .pixelFormat = format,
          },
  };
  auto texture = std::make_shared<Texture>(std::move(label));
  texture->init(desc);
  return texture;
}

Texture &Texture::init(const TextureDesc &desc) {
  contextData.reset();
  this->desc = desc;
  return *this;
}

Texture &Texture::initWithSamplerState(const SamplerState &samplerState) {
  desc.samplerState = samplerState;
  contextData.reset();
  return *this;
}

Texture &Texture::initWithResolution(int2 resolution) {
  if (desc.resolution != resolution) {
    desc.resolution = resolution;
    contextData.reset();
  }
  return *this;
}

Texture &Texture::initWithFlags(TextureFormatFlags flags) {
  if (desc.format.flags != flags) {
    desc.format.flags = flags;
    contextData.reset();
  }
  return *this;
}

Texture &Texture::initWithPixelFormat(WGPUTextureFormat format) {
  if (desc.format.pixelFormat != format) {
    desc.format.pixelFormat = format;
    contextData.reset();
  }
  return *this;
}

Texture &Texture::initWithLabel(std::string &&label) {
  this->label = std::move(label);
  contextData.reset();
  return *this;
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
  const TextureFormatDesc &inputFormat = getTextureFormatDescription(format.pixelFormat);
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

  contextData.size.width = desc.resolution.x;
  contextData.size.height = desc.resolution.y;
  contextData.size.depthOrArrayLayers = 1;
  contextData.format = desc.format;

  if (contextData.size.width == 0 || contextData.size.height == 0)
    return;

  if (desc.externalTexture) {
    contextData.defaultView = desc.externalTexture.value();
    contextData.isExternalView = true;
  } else {
    WGPUTextureDescriptor wgpuDesc = {};
    wgpuDesc.usage = WGPUTextureUsage_TextureBinding;
    if (desc.data) {
      wgpuDesc.usage |= WGPUTextureUsage_CopyDst;
    }

    if (textureFormatFlagsContains(desc.format.flags, TextureFormatFlags::RenderAttachment)) {
      wgpuDesc.usage |= WGPUTextureUsage_RenderAttachment;
    }

    switch (desc.format.type) {
    case TextureType::D1:
      wgpuDesc.dimension = WGPUTextureDimension_1D;
      contextData.size.height = 1;
      break;
    case TextureType::D2:
      wgpuDesc.dimension = WGPUTextureDimension_2D;
      break;
    case TextureType::Cube:
      wgpuDesc.dimension = WGPUTextureDimension_2D;
      contextData.size.depthOrArrayLayers = 8;
      break;
    default:
      assert(false);
    }

    wgpuDesc.size = contextData.size;
    wgpuDesc.format = desc.format.pixelFormat;
    wgpuDesc.sampleCount = 1;
    wgpuDesc.mipLevelCount = 1;
    wgpuDesc.label = label.empty() ? "unknown" : label.c_str();

    contextData.texture = wgpuDeviceCreateTexture(context.wgpuDevice, &wgpuDesc);
    assert(contextData.texture);

    if (desc.data)
      writeTextureData(context, desc.format, desc.resolution, contextData.texture, desc.data);

    contextData.defaultView = createView(desc.format, contextData.texture);
    assert(contextData.defaultView);
  }

  contextData.sampler = createSampler(context, desc.samplerState, false);
  assert(contextData.sampler);
}

void Texture::updateContextData(Context &context, TextureContextData &contextData) {}

} // namespace gfx
