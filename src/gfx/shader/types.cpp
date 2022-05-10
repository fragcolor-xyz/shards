#include "types.hpp"
#include <nameof.hpp>
#include <stdexcept>

namespace gfx {
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
size_t getByteSize(const ShaderFieldBaseType &type) {
  switch (type) {
  case ShaderFieldBaseType::UInt8:
  case ShaderFieldBaseType::Int8:
    return 1;
  case ShaderFieldBaseType::UInt16:
  case ShaderFieldBaseType::Int16:
  case ShaderFieldBaseType::Float16:
    return 2;
  case ShaderFieldBaseType::UInt32:
  case ShaderFieldBaseType::Int32:
  case ShaderFieldBaseType::Float32:
    return 4;
  default:
    throw std::out_of_range(std::string(NAMEOF_TYPE(ShaderFieldBaseType)));
  }
}

size_t getWGSLAlignment(const ShaderFieldBaseType &type) {
  // https://www.w3.org/TR/WGSL/#alignment-and-size
  switch (type) {
  case ShaderFieldBaseType::UInt8:
  case ShaderFieldBaseType::Int8:
  case ShaderFieldBaseType::UInt16:
  case ShaderFieldBaseType::Int16:
    throw std::logic_error("Not a valid host-sharable type");
  case ShaderFieldBaseType::UInt32:
  case ShaderFieldBaseType::Int32:
  case ShaderFieldBaseType::Float32:
    return 4;
  case ShaderFieldBaseType::Float16:
    return 2;
  default:
    throw std::out_of_range(std::string(NAMEOF_TYPE(ShaderFieldBaseType)));
  }
}

namespace shader {
size_t FieldType::getByteSize() const { return gfx::getByteSize(baseType) * numComponents; }
size_t FieldType::getWGSLAlignment() const {
  // https://www.w3.org/TR/WGSL/#alignment-and-size
  size_t componentAlignment = gfx::getWGSLAlignment(baseType);
  switch (numComponents) {
  case 1:
    return componentAlignment;
  case 2:
    return componentAlignment * 2;
  case 3:
    return componentAlignment * 4;
  case 4:
  case FieldType::Float4x4Components:
    return componentAlignment * 4;
  default:
    throw std::logic_error("Not a valid host-sharable type");
  }
}
} // namespace shader
} // namespace gfx