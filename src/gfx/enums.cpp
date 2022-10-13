#include "enums.hpp"
#include "error_utils.hpp"
#include <magic_enum.hpp>
#include <nameof.hpp>
#include <stdexcept>
#include <stdint.h>
#include <map>

namespace gfx {

typedef std::map<WGPUTextureFormat, TextureFormatDesc> TextureFormatMap;

TextureFormatDesc::TextureFormatDesc(StorageType storageType, size_t numComponents, TextureFormatUsage usage)
    : storageType(storageType), numComponents(numComponents), usage(usage) {
  if (storageType == StorageType::Invalid)
    pixelSize = 0;
  else
    pixelSize = getStorageTypeSize(storageType) * numComponents;
}

static const TextureFormatMap &getTextureFormatMap() {
  static TextureFormatMap instance = []() {
    return TextureFormatMap{
        {WGPUTextureFormat_R8Unorm, {StorageType::UNorm8, 1}},
        // u8x1
        {WGPUTextureFormat_R8Snorm, {StorageType::SNorm8, 1}},
        {WGPUTextureFormat_R8Uint, {StorageType::UInt8, 1}},
        {WGPUTextureFormat_R8Sint, {StorageType::Int8, 1}},
        // u8x2
        {WGPUTextureFormat_RG8Unorm, {StorageType::UNorm8, 2}},
        {WGPUTextureFormat_RG8Snorm, {StorageType::SNorm8, 2}},
        {WGPUTextureFormat_RG8Uint, {StorageType::UInt8, 2}},
        {WGPUTextureFormat_RG8Sint, {StorageType::Int8, 2}},
        // u8x4
        {WGPUTextureFormat_RGBA8Unorm, {StorageType::UNorm8, 4}},
        {WGPUTextureFormat_RGBA8UnormSrgb, {StorageType::UNorm8, 4}},
        {WGPUTextureFormat_RGBA8Snorm, {StorageType::SNorm8, 4}},
        {WGPUTextureFormat_RGBA8Uint, {StorageType::UInt8, 4}},
        {WGPUTextureFormat_RGBA8Sint, {StorageType::Int8, 4}},
        {WGPUTextureFormat_BGRA8Unorm, {StorageType::UNorm8, 4}},
        {WGPUTextureFormat_BGRA8UnormSrgb, {StorageType::UNorm8, 4}},
        // u16x1
        {WGPUTextureFormat_R16Uint, {StorageType::UInt16, 1}},
        {WGPUTextureFormat_R16Sint, {StorageType::Int16, 1}},
        {WGPUTextureFormat_R16Float, {StorageType::Float16, 1}},
        // u16x2
        {WGPUTextureFormat_RG16Uint, {StorageType::UInt16, 2}},
        {WGPUTextureFormat_RG16Sint, {StorageType::Int16, 2}},
        {WGPUTextureFormat_RG16Float, {StorageType::Float16, 2}},
        // u16x4
        {WGPUTextureFormat_RGBA16Uint, {StorageType::UInt16, 4}},
        {WGPUTextureFormat_RGBA16Sint, {StorageType::Int16, 4}},
        {WGPUTextureFormat_RGBA16Float, {StorageType::Float16, 4}},
        // u32x1
        {WGPUTextureFormat_R32Float, {StorageType::UInt32, 1}},
        {WGPUTextureFormat_R32Uint, {StorageType::Int32, 1}},
        {WGPUTextureFormat_R32Sint, {StorageType::Float32, 1}},
        // u32x2
        {WGPUTextureFormat_RG32Float, {StorageType::UInt32, 2}},
        {WGPUTextureFormat_RG32Uint, {StorageType::Int32, 2}},
        {WGPUTextureFormat_RG32Sint, {StorageType::Float32, 2}},
        // u32x4
        {WGPUTextureFormat_RGBA32Float, {StorageType::UInt32, 4}},
        {WGPUTextureFormat_RGBA32Uint, {StorageType::Int32, 4}},
        {WGPUTextureFormat_RGBA32Sint, {StorageType::Float32, 4}},
        // Depth(+stencil) formats
        {WGPUTextureFormat_Depth32Float, {StorageType::Invalid, 0, TextureFormatUsage::Depth}},
        {WGPUTextureFormat_Depth24Plus, {StorageType::Invalid, 0, TextureFormatUsage::Depth}},
        {WGPUTextureFormat_Depth16Unorm, {StorageType::UNorm16, 2, TextureFormatUsage::Depth}},
        {WGPUTextureFormat_Stencil8, {StorageType::UInt8, 1, TextureFormatUsage::Stencil}},
        {WGPUTextureFormat_Depth32FloatStencil8,
         {StorageType::Invalid, 0, TextureFormatUsage::Depth | TextureFormatUsage::Stencil}},
        {WGPUTextureFormat_Depth24PlusStencil8,
         {StorageType::Invalid, 0, TextureFormatUsage::Depth | TextureFormatUsage::Stencil}},
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
} // namespace gfx
