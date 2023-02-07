#ifndef C9D0BB09_616F_4DAF_BFFF_4328B7893E6A
#define C9D0BB09_616F_4DAF_BFFF_4328B7893E6A

#include "../enums.hpp"
#include "fwd.hpp"
#include <optional>
#include <string>

namespace gfx {

// TODO: Rename to BaseType
// TODO: Move to shader namespace
enum class ShaderFieldBaseType { Bool, UInt8, Int8, UInt16, Int16, UInt32, Int32, Float16, Float32 };
enum class ShaderTextureFormat {
  Int32,
  UInt32,
  Float32,
};
bool isIntegerType(const ShaderFieldBaseType &type);
bool isFloatType(const ShaderFieldBaseType &type);
size_t getByteSize(const ShaderFieldBaseType &type);
size_t getWGSLAlignment(const ShaderFieldBaseType &type);

namespace shader {
struct TextureFieldType {
  ShaderTextureFormat format;
  gfx::TextureDimension dimension;
};

// Structure to represent wgsl numerical types (numbers, vectors, matrices)
struct FieldType {
  // Unit type of the vector/scalar
  ShaderFieldBaseType baseType = ShaderFieldBaseType::Float32;

  // Number of component in the vector/scalar type
  size_t numComponents = 1;

  // 1 for scalar/vectors
  // higher for matrixes
  // float4x4 will have a matrix dimension of 4 and a numComponents of 4
  // float3x3 will have a matrix dimension of 3 and a numComponents of 3
  // etc.
  size_t matrixDimension = 1;

  FieldType() = default;
  FieldType(ShaderFieldBaseType baseType) : baseType(baseType), numComponents(1) {}
  FieldType(ShaderFieldBaseType baseType, size_t numComponents, size_t matrixDimension = 1)
      : baseType(baseType), numComponents(numComponents), matrixDimension(matrixDimension) {}
  bool operator==(const FieldType &other) const {
    return baseType == other.baseType && numComponents == other.numComponents && matrixDimension == other.matrixDimension;
  }
  bool operator!=(const FieldType &other) const { return !(*this == other); }

  size_t getByteSize() const;
  size_t getWGSLAlignment() const;
};

/// <div rustbindgen hide></div>
struct FieldTypes {
  static inline FieldType Float{ShaderFieldBaseType::Float32};
  static inline FieldType Float2{ShaderFieldBaseType::Float32, 2};
  static inline FieldType Float3{ShaderFieldBaseType::Float32, 3};
  static inline FieldType Float4{ShaderFieldBaseType::Float32, 4};
  static inline FieldType Float2x2{ShaderFieldBaseType::Float32, 2, 2};
  static inline FieldType Float3x3{ShaderFieldBaseType::Float32, 3, 3};
  static inline FieldType Float4x4{ShaderFieldBaseType::Float32, 4, 4};
  static inline FieldType UInt32{ShaderFieldBaseType::UInt32, 1};
  static inline FieldType Int32{ShaderFieldBaseType::Int32, 1};
  static inline FieldType Bool{ShaderFieldBaseType::Bool, 1};
};

struct NamedField {
  String name;
  FieldType type;

  NamedField() = default;
  NamedField(String name, const FieldType &type) : name(name), type(type) {}
  NamedField(String name, FieldType &&type) : name(name), type(type) {}
};

} // namespace shader
} // namespace gfx

#endif /* C9D0BB09_616F_4DAF_BFFF_4328B7893E6A */
