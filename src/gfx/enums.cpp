#include "enums.hpp"
#include <magic_enum.hpp>
#include <nameof.hpp>
#include <stdexcept>
#include <stdint.h>

namespace gfx {
size_t getVertexAttributeTypeSize(const VertexAttributeType &type) {
  switch (type) {
  case VertexAttributeType::UInt8:
  case VertexAttributeType::Int8:
  case VertexAttributeType::UNorm8:
  case VertexAttributeType::SNorm8:
    return sizeof(uint8_t);
  case VertexAttributeType::UInt16:
  case VertexAttributeType::Int16:
  case VertexAttributeType::UNorm16:
  case VertexAttributeType::SNorm16:
    return sizeof(uint16_t);
  case VertexAttributeType::UInt32:
  case VertexAttributeType::Int32:
    return sizeof(uint32_t);
  case VertexAttributeType::Float16:
    return sizeof(uint16_t);
  case VertexAttributeType::Float32:
    return sizeof(float);
  default:
    throw std::out_of_range(std::string(NAMEOF_TYPE(VertexAttributeType)));
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

bool isFloatType(const ShaderFieldBaseType &type) {
  switch (type) {
  case ShaderFieldBaseType::UInt8:
  case ShaderFieldBaseType::Int8:
  case ShaderFieldBaseType::UInt16:
  case ShaderFieldBaseType::Int16:
  case ShaderFieldBaseType::UInt32:
  case ShaderFieldBaseType::Int32:
    return false;
  case ShaderFieldBaseType::Float16:
  case ShaderFieldBaseType::Float32:
    return true;
  default:
    throw std::out_of_range(std::string(NAMEOF_TYPE(ShaderFieldBaseType)));
  }
}

bool isIntegerType(const ShaderFieldBaseType &type) { return !isFloatType(type); }

WGPUVertexFormat getWGPUVertexFormat(const VertexAttributeType &type, size_t dim) {
  switch (type) {
  case VertexAttributeType::UInt8:
    if (dim == 2)
      return WGPUVertexFormat_Uint8x2;
    else if (dim == 4)
      return WGPUVertexFormat_Uint8x4;
    else {
      throw std::out_of_range("VertexAttributeType/dim");
    }
  case VertexAttributeType::Int8:
    if (dim == 2)
      return WGPUVertexFormat_Sint8x2;
    else if (dim == 4)
      return WGPUVertexFormat_Sint8x4;
    else {
      throw std::out_of_range("VertexAttributeType/dim");
    }
  case VertexAttributeType::UNorm8:
    if (dim == 2)
      return WGPUVertexFormat_Unorm8x2;
    else if (dim == 4)
      return WGPUVertexFormat_Unorm8x4;
    else {
      throw std::out_of_range("VertexAttributeType/dim");
    }
  case VertexAttributeType::SNorm8:
    if (dim == 2)
      return WGPUVertexFormat_Snorm8x2;
    else if (dim == 4)
      return WGPUVertexFormat_Snorm8x4;
    else {
      throw std::out_of_range("VertexAttributeType/dim");
    }
  case VertexAttributeType::UInt16:

    if (dim == 2)
      return WGPUVertexFormat_Uint16x2;
    else if (dim == 4)
      return WGPUVertexFormat_Uint16x4;
    else {
      throw std::out_of_range("VertexAttributeType/dim");
    }
  case VertexAttributeType::Int16:

    if (dim == 2)
      return WGPUVertexFormat_Sint16x2;
    else if (dim == 4)
      return WGPUVertexFormat_Sint16x4;
    else {
      throw std::out_of_range("VertexAttributeType/dim");
    }
  case VertexAttributeType::UNorm16:

    if (dim == 2)
      return WGPUVertexFormat_Unorm16x2;
    else if (dim == 4)
      return WGPUVertexFormat_Unorm16x4;
    else {
      throw std::out_of_range("VertexAttributeType/dim");
    }
  case VertexAttributeType::SNorm16:

    if (dim == 2)
      return WGPUVertexFormat_Snorm16x2;
    else if (dim == 4)
      return WGPUVertexFormat_Snorm16x4;
    else {
      throw std::out_of_range("VertexAttributeType/dim");
    }
  case VertexAttributeType::UInt32:

    if (dim == 1) {
      return WGPUVertexFormat_Uint32;
    } else if (dim == 2)
      return WGPUVertexFormat_Uint32x2;
    else if (dim == 3)
      return WGPUVertexFormat_Uint32x3;
    else if (dim == 4)
      return WGPUVertexFormat_Uint32x4;
    else {
      throw std::out_of_range("VertexAttributeType/dim");
    }
  case VertexAttributeType::Int32:
    if (dim == 1) {
      return WGPUVertexFormat_Sint32;
    } else if (dim == 2)
      return WGPUVertexFormat_Sint32x2;
    else if (dim == 3)
      return WGPUVertexFormat_Sint32x3;
    else if (dim == 4)
      return WGPUVertexFormat_Sint32x4;
    else {
      throw std::out_of_range("VertexAttributeType/dim");
    }
  case VertexAttributeType::Float16:
    if (dim == 2)
      return WGPUVertexFormat_Float16x2;
    else if (dim == 4)
      return WGPUVertexFormat_Float16x4;
    else {
      throw std::out_of_range("VertexAttributeType/dim");
    }
  case VertexAttributeType::Float32:
    if (dim == 1) {
      return WGPUVertexFormat_Float32;
    } else if (dim == 2)
      return WGPUVertexFormat_Float32x2;
    else if (dim == 3)
      return WGPUVertexFormat_Float32x3;
    else if (dim == 4)
      return WGPUVertexFormat_Float32x4;
    else {
      throw std::out_of_range("VertexAttributeType/dim");
    }
  default:
    throw std::out_of_range(std::string(NAMEOF_TYPE(VertexAttributeType)));
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
