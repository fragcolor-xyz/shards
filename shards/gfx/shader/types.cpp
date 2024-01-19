#include "types.hpp"
#include "../math.hpp"
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
size_t NumType::getByteSize() const {
  size_t vecSize = gfx::getByteSize(baseType) * numComponents;

  if (matrixDimension > 1) {
    // Matrix size is based on array of vector elements
    // https://www.w3.org/TR/WGSL/#alignment-and-size
    size_t vecAlignment = NumType(baseType, numComponents).getWGSLAlignment();
    return matrixDimension * alignToPOT(vecSize, vecAlignment);
  } else {
    return vecSize;
  }
}
size_t NumType::getWGSLAlignment() const {
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
    return componentAlignment * 4;
  default:
    throw std::logic_error("Not a valid host-sharable type");
  }
}
} // namespace shader
} // namespace gfx