#include "enums.hpp"
#include "error_utils.hpp"
#include <magic_enum.hpp>
#include <nameof.hpp>
#include <stdexcept>
#include <stdint.h>
#include <map>

namespace gfx {

typedef std::map<WGPUTextureFormat, TextureFormatDesc> TextureFormatMap;

TextureFormatDesc::TextureFormatDesc(StorageType storageType, size_t numComponents, TextureSampleType compatibleSampleTypes,
                                     ColorSpace colorSpace, TextureFormatUsage usage)
    : storageType(storageType), numComponents(numComponents), compatibleSampleTypes(compatibleSampleTypes),
      colorSpace(colorSpace), usage(usage) {
  if (storageType == StorageType::Invalid)
    pixelSize = 0;
  else
    pixelSize = getStorageTypeSize(storageType) * numComponents;
}

static const TextureFormatMap &getTextureFormatMap() {
  static TextureFormatMap instance = []() {
    return TextureFormatMap{
        // u8x1
        {WGPUTextureFormat_R8Unorm, {StorageType::UNorm8, 1, TextureSampleType::Float}},
        {WGPUTextureFormat_R8Snorm, {StorageType::SNorm8, 1, TextureSampleType::Float}},
        {WGPUTextureFormat_R8Uint, {StorageType::UInt8, 1, TextureSampleType::UInt}},
        {WGPUTextureFormat_R8Sint, {StorageType::Int8, 1, TextureSampleType::Int}},
        // u8x2
        {WGPUTextureFormat_RG8Unorm, {StorageType::UNorm8, 2, TextureSampleType::Float}},
        {WGPUTextureFormat_RG8Snorm, {StorageType::SNorm8, 2, TextureSampleType::Float}},
        {WGPUTextureFormat_RG8Uint, {StorageType::UInt8, 2, TextureSampleType::UInt}},
        {WGPUTextureFormat_RG8Sint, {StorageType::Int8, 2, TextureSampleType::Int}},
        // u8x4
        {WGPUTextureFormat_RGBA8Unorm, {StorageType::UNorm8, 4, TextureSampleType::Float}},
        {WGPUTextureFormat_RGBA8UnormSrgb, {StorageType::UNorm8, 4, TextureSampleType::Float, ColorSpace::Srgb}},
        {WGPUTextureFormat_RGBA8Snorm, {StorageType::SNorm8, 4, TextureSampleType::Float}},
        {WGPUTextureFormat_RGBA8Uint, {StorageType::UInt8, 4, TextureSampleType::UInt}},
        {WGPUTextureFormat_RGBA8Sint, {StorageType::Int8, 4, TextureSampleType::Int}},
        {WGPUTextureFormat_BGRA8Unorm, {StorageType::UNorm8, 4, TextureSampleType::Float}},
        {WGPUTextureFormat_BGRA8UnormSrgb, {StorageType::UNorm8, 4, TextureSampleType::Float, ColorSpace::Srgb}},
        // u16x1
        {WGPUTextureFormat_R16Uint, {StorageType::UInt16, 1, TextureSampleType::UInt}},
        {WGPUTextureFormat_R16Sint, {StorageType::Int16, 1, TextureSampleType::Int}},
        {WGPUTextureFormat_R16Float, {StorageType::Float16, 1, TextureSampleType::Float}},
        // u16x2
        {WGPUTextureFormat_RG16Uint, {StorageType::UInt16, 2, TextureSampleType::UInt}},
        {WGPUTextureFormat_RG16Sint, {StorageType::Int16, 2, TextureSampleType::Int}},
        {WGPUTextureFormat_RG16Float, {StorageType::Float16, 2, TextureSampleType::Float}},
        // u16x4
        {WGPUTextureFormat_RGBA16Uint, {StorageType::UInt16, 4, TextureSampleType::UInt}},
        {WGPUTextureFormat_RGBA16Sint, {StorageType::Int16, 4, TextureSampleType::Int}},
        {WGPUTextureFormat_RGBA16Float, {StorageType::Float16, 4, TextureSampleType::Float}},
        // u32x1
        {WGPUTextureFormat_R32Uint, {StorageType::UInt32, 1, TextureSampleType::UInt}},
        {WGPUTextureFormat_R32Sint, {StorageType::Int32, 1, TextureSampleType::Int}},
        {WGPUTextureFormat_R32Float, {StorageType::Float32, 1, TextureSampleType::UnfilterableFloat}},
        // u32x2
        {WGPUTextureFormat_RG32Uint, {StorageType::UInt32, 2, TextureSampleType::UInt}},
        {WGPUTextureFormat_RG32Sint, {StorageType::Int32, 2, TextureSampleType::Int}},
        {WGPUTextureFormat_RG32Float, {StorageType::Float32, 2, TextureSampleType::UnfilterableFloat}},
        // u32x4
        {WGPUTextureFormat_RGBA32Uint, {StorageType::UInt32, 4, TextureSampleType::UInt}},
        {WGPUTextureFormat_RGBA32Sint, {StorageType::Int32, 4, TextureSampleType::Int}},
        {WGPUTextureFormat_RGBA32Float, {StorageType::Float32, 4, TextureSampleType::UnfilterableFloat}},
        // Depth(+stencil) formats
        {WGPUTextureFormat_Depth32Float,
         {StorageType::Invalid, 0, TextureSampleType::UnfilterableFloat, ColorSpace::Undefined, TextureFormatUsage::Depth}},
        {WGPUTextureFormat_Depth24Plus,
         {StorageType::Invalid, 0, TextureSampleType::UnfilterableFloat, ColorSpace::Undefined, TextureFormatUsage::Depth}},
        {WGPUTextureFormat_Depth16Unorm,
         {StorageType::UNorm16, 2, TextureSampleType::UnfilterableFloat, ColorSpace::Undefined, TextureFormatUsage::Depth}},
        {WGPUTextureFormat_Stencil8,
         {StorageType::UInt8, 1, TextureSampleType::UInt, ColorSpace::Undefined, TextureFormatUsage::Stencil}},
        {WGPUTextureFormat_Depth32FloatStencil8,
         {StorageType::Invalid, 0, TextureSampleType::UnfilterableFloat, ColorSpace::Undefined,
          TextureFormatUsage::Depth | TextureFormatUsage::Stencil}},
        {WGPUTextureFormat_Depth24PlusStencil8,
         {StorageType::Invalid, 0, TextureSampleType::UnfilterableFloat, ColorSpace::Undefined,
          TextureFormatUsage::Depth | TextureFormatUsage::Stencil}},
    };
  }();
  return instance;
}

const TextureFormatDesc &getTextureFormatDescription(WGPUTextureFormat pixelFormat) {
  auto &textureFormatMap = getTextureFormatMap();
  auto it = textureFormatMap.find(pixelFormat);
  if (it == textureFormatMap.end()) {
    throw formatException("Unsupported texture input format", magic_enum::enum_name(pixelFormat));
  }
  return it->second;
}

std::optional<WGPUTextureFormat> upgradeToSrgbFormat(WGPUTextureFormat pixelFormat) {
  switch (pixelFormat) {
  case WGPUTextureFormat_RGBA8Unorm:
    return WGPUTextureFormat_RGBA8UnormSrgb;
  case WGPUTextureFormat_BGRA8Unorm:
    return WGPUTextureFormat_BGRA8UnormSrgb;
  default:
    return std::nullopt;
  }
}

size_t getStorageTypeSize(const StorageType &type) {
  switch (type) {
  case StorageType::UInt8:
  case StorageType::Int8:
  case StorageType::UNorm8:
  case StorageType::SNorm8:
    return sizeof(uint8_t);
  case StorageType::UInt16:
  case StorageType::Int16:
  case StorageType::UNorm16:
  case StorageType::SNorm16:
    return sizeof(uint16_t);
  case StorageType::UInt32:
  case StorageType::Int32:
    return sizeof(uint32_t);
  case StorageType::Float16:
    return sizeof(uint16_t);
  case StorageType::Float32:
    return sizeof(float);
  default:
    throw std::out_of_range(std::string(NAMEOF_TYPE(StorageType)));
  }
}

bool isIntegerStorageType(const StorageType &type) {
  switch (type) {
  case StorageType::UInt8:
  case StorageType::Int8:
  case StorageType::UNorm8:
  case StorageType::SNorm8:
  case StorageType::UInt16:
  case StorageType::Int16:
  case StorageType::UNorm16:
  case StorageType::SNorm16:
  case StorageType::UInt32:
  case StorageType::Int32:
    return true;
  case StorageType::Float16:
  case StorageType::Float32:
    return false;
  default:
    throw std::out_of_range(std::string(NAMEOF_TYPE(StorageType)));
  }
}

size_t getIndexFormatSize(const IndexFormat &type) {
  switch (type) {
  case IndexFormat::UInt16:
    return sizeof(uint16_t);
  case IndexFormat::UInt32:
    return sizeof(uint32_t);
  default:
    throw std::out_of_range(std::string(NAMEOF_TYPE(IndexFormat)));
  }
}

WGPUVertexFormat getWGPUVertexFormat(const StorageType &type, size_t dim) {
  switch (type) {
  case StorageType::UInt8:
    if (dim == 2)
      return WGPUVertexFormat_Uint8x2;
    else if (dim == 4)
      return WGPUVertexFormat_Uint8x4;
    else {
      throw std::out_of_range("StorageType/dim");
    }
  case StorageType::Int8:
    if (dim == 2)
      return WGPUVertexFormat_Sint8x2;
    else if (dim == 4)
      return WGPUVertexFormat_Sint8x4;
    else {
      throw std::out_of_range("StorageType/dim");
    }
  case StorageType::UNorm8:
    if (dim == 2)
      return WGPUVertexFormat_Unorm8x2;
    else if (dim == 4)
      return WGPUVertexFormat_Unorm8x4;
    else {
      throw std::out_of_range("StorageType/dim");
    }
  case StorageType::SNorm8:
    if (dim == 2)
      return WGPUVertexFormat_Snorm8x2;
    else if (dim == 4)
      return WGPUVertexFormat_Snorm8x4;
    else {
      throw std::out_of_range("StorageType/dim");
    }
  case StorageType::UInt16:

    if (dim == 2)
      return WGPUVertexFormat_Uint16x2;
    else if (dim == 4)
      return WGPUVertexFormat_Uint16x4;
    else {
      throw std::out_of_range("StorageType/dim");
    }
  case StorageType::Int16:

    if (dim == 2)
      return WGPUVertexFormat_Sint16x2;
    else if (dim == 4)
      return WGPUVertexFormat_Sint16x4;
    else {
      throw std::out_of_range("StorageType/dim");
    }
  case StorageType::UNorm16:

    if (dim == 2)
      return WGPUVertexFormat_Unorm16x2;
    else if (dim == 4)
      return WGPUVertexFormat_Unorm16x4;
    else {
      throw std::out_of_range("StorageType/dim");
    }
  case StorageType::SNorm16:

    if (dim == 2)
      return WGPUVertexFormat_Snorm16x2;
    else if (dim == 4)
      return WGPUVertexFormat_Snorm16x4;
    else {
      throw std::out_of_range("StorageType/dim");
    }
  case StorageType::UInt32:

    if (dim == 1) {
      return WGPUVertexFormat_Uint32;
    } else if (dim == 2)
      return WGPUVertexFormat_Uint32x2;
    else if (dim == 3)
      return WGPUVertexFormat_Uint32x3;
    else if (dim == 4)
      return WGPUVertexFormat_Uint32x4;
    else {
      throw std::out_of_range("StorageType/dim");
    }
  case StorageType::Int32:
    if (dim == 1) {
      return WGPUVertexFormat_Sint32;
    } else if (dim == 2)
      return WGPUVertexFormat_Sint32x2;
    else if (dim == 3)
      return WGPUVertexFormat_Sint32x3;
    else if (dim == 4)
      return WGPUVertexFormat_Sint32x4;
    else {
      throw std::out_of_range("StorageType/dim");
    }
  case StorageType::Float16:
    if (dim == 2)
      return WGPUVertexFormat_Float16x2;
    else if (dim == 4)
      return WGPUVertexFormat_Float16x4;
    else {
      throw std::out_of_range("StorageType/dim");
    }
  case StorageType::Float32:
    if (dim == 1) {
      return WGPUVertexFormat_Float32;
    } else if (dim == 2)
      return WGPUVertexFormat_Float32x2;
    else if (dim == 3)
      return WGPUVertexFormat_Float32x3;
    else if (dim == 4)
      return WGPUVertexFormat_Float32x4;
    else {
      throw std::out_of_range("StorageType/dim");
    }
  default:
    throw std::out_of_range(std::string(NAMEOF_TYPE(StorageType)));
  }
}
WGPUIndexFormat getWGPUIndexFormat(const IndexFormat &type) {
  switch (type) {
  case IndexFormat::UInt16:
    return WGPUIndexFormat_Uint16;
  case IndexFormat::UInt32:
    return WGPUIndexFormat_Uint32;
  default:
    throw std::out_of_range(std::string(NAMEOF_TYPE(IndexFormat)));
  }
}

WGPUTextureSampleType getWGPUSampleType(TextureSampleType type) {
  switch (type) {
  case TextureSampleType::Float:
    return WGPUTextureSampleType_Float;
  case TextureSampleType::Int:
    return WGPUTextureSampleType_Sint;
  case TextureSampleType::UInt:
    return WGPUTextureSampleType_Uint;
  case TextureSampleType::UnfilterableFloat:
    return WGPUTextureSampleType_UnfilterableFloat;
  default:
    throw std::out_of_range(std::string(NAMEOF_TYPE(TextureSampleType)));
  }
}
} // namespace gfx
