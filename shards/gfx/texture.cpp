#include "texture.hpp"
#include "context.hpp"
#include "error_utils.hpp"
#include "gfx_wgpu.hpp"
#include <tracy/Wrapper.hpp>
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
  if (this->desc != desc || desc.externalTexture) {
    // NOTE: Always update when external texture is present
    update();
    this->desc = desc;
  }
  return *this;
}

Texture &Texture::initWithSamplerState(const SamplerState &samplerState) {
  if (this->samplerState != samplerState) {
    this->samplerState = samplerState;
    ++version;
  }
  return *this;
}

Texture &Texture::initWithResolution(int2 resolution) {
  if (desc.resolution != resolution) {
    desc.resolution = resolution;
    update();
  }
  return *this;
}

Texture &Texture::initWithFlags(TextureFormatFlags flags) {
  if (desc.format.flags != flags) {
    desc.format.flags = flags;
    update();
  }
  return *this;
}

Texture &Texture::initWithPixelFormat(WGPUTextureFormat format) {
  if (desc.format.pixelFormat != format) {
    desc.format.pixelFormat = format;
    update();
  }
  return *this;
}

Texture &Texture::initWithLabel(std::string &&label) {
  this->label = std::move(label);
  update();
  return *this;
}

std::shared_ptr<Texture> Texture::clone() const {
  auto result = cloneSelfWithId(this, getNextId());
  result->update();
  return result;
}

static void writeTextureData(Context &context, const TextureFormat &format, const int2 &resolution, uint32_t numArrayLayers,
                             WGPUTexture texture, const ImmutableSharedBuffer &isb) {
  ZoneScoped;

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
      .depthOrArrayLayers = numArrayLayers,
  };
  wgpuQueueWriteTexture(context.wgpuQueue, &dst, isb.getData(), isb.getLength(), &layout, &writeSize);
}

void Texture::initContextData(Context &context, TextureContextData &contextData) {
  ZoneScoped;

  WGPUDevice device = context.wgpuDevice;
  shassert(device);

  contextData.id = id;
  contextData.version = version;
  contextData.size.width = desc.resolution.x;
  contextData.size.height = desc.resolution.y;
  contextData.size.depthOrArrayLayers = 1;
  contextData.format = desc.format;

  if (contextData.size.width == 0 || contextData.size.height == 0)
    return;

  if (desc.externalTexture) {
    contextData.externalTexture = desc.externalTexture.value();
  } else {
    WGPUTextureDescriptor wgpuDesc = {};
    wgpuDesc.usage = WGPUTextureUsage_TextureBinding;
    if (desc.data) {
      wgpuDesc.usage |= WGPUTextureUsage_CopyDst;
    }

    if (textureFormatFlagsContains(desc.format.flags, TextureFormatFlags::RenderAttachment)) {
      wgpuDesc.usage |= WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_CopySrc;
    }

    if (!textureFormatFlagsContains(desc.format.flags, TextureFormatFlags::NoTextureBinding)) {
      wgpuDesc.usage |= WGPUTextureUsage_TextureBinding;
    }

    switch (desc.format.dimension) {
    case TextureDimension::D1:
      wgpuDesc.dimension = WGPUTextureDimension_1D;
      contextData.size.height = 1;
      break;
    case TextureDimension::D2:
      wgpuDesc.dimension = WGPUTextureDimension_2D;
      break;
    case TextureDimension::Cube:
      wgpuDesc.dimension = WGPUTextureDimension_2D;
      contextData.size.depthOrArrayLayers = 6;
      break;
    default:
      shassert(false);
    }

    wgpuDesc.size = contextData.size;
    wgpuDesc.format = desc.format.pixelFormat;
    wgpuDesc.sampleCount = 1;
    wgpuDesc.mipLevelCount = desc.format.mipLevels;
    wgpuDesc.label = label.empty() ? "unknown" : label.c_str();

    contextData.texture = wgpuDeviceCreateTexture(context.wgpuDevice, &wgpuDesc);
    shassert(contextData.texture);

    if (desc.data)
      writeTextureData(context, desc.format, desc.resolution, contextData.size.depthOrArrayLayers, contextData.texture,
                       desc.data);
  }
}

void Texture::updateContextData(Context &context, TextureContextData &contextData) {}

UniqueId Texture::getNextId() {
  static UniqueIdGenerator gen(UniqueIdTag::Texture);
  return gen.getNext();
}

void Texture::update() {
  ++version;
  resetContextData();
}

} // namespace gfx
