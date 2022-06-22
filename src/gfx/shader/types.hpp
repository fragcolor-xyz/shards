#ifndef GFX_SHADER_TYPES
#define GFX_SHADER_TYPES

#include "fwd.hpp"
#include <optional>
#include <string>

namespace gfx {

// TODO: Rename to BaseType
// TODO: Move to shader namespace
enum class ShaderFieldBaseType { UInt8, Int8, UInt16, Int16, UInt32, Int32, Float16, Float32 };
bool isIntegerType(const ShaderFieldBaseType &type);
bool isFloatType(const ShaderFieldBaseType &type);
size_t getByteSize(const ShaderFieldBaseType &type);
size_t getWGSLAlignment(const ShaderFieldBaseType &type);

namespace shader {

enum struct UniformShard {
  View,
  Object,
};

// Structure to represent wgsl numerical types (numbers, vectors, matrices)
struct FieldType {
  // Special value for numCompontens to specify a square 4x4 matrix
  static constexpr inline size_t Float4x4Components = 16;

  ShaderFieldBaseType baseType = ShaderFieldBaseType::Float32;
  size_t numComponents = 1;

  FieldType() = default;
  FieldType(ShaderFieldBaseType baseType) : baseType(baseType), numComponents(1) {}
  FieldType(ShaderFieldBaseType baseType, size_t numComponents) : baseType(baseType), numComponents(numComponents) {}
  bool operator==(const FieldType &other) const { return baseType == other.baseType && numComponents == other.numComponents; }
  bool operator!=(const FieldType &other) const { return !(*this == other); }

  size_t getByteSize() const;
  size_t getWGSLAlignment() const;
};

struct FieldTypes {
  static inline FieldType Float{ShaderFieldBaseType::Float32};
  static inline FieldType Float2{ShaderFieldBaseType::Float32, 2};
  static inline FieldType Float3{ShaderFieldBaseType::Float32, 3};
  static inline FieldType Float4{ShaderFieldBaseType::Float32, 4};
  static inline FieldType Float4x4{ShaderFieldBaseType::Float32, FieldType::Float4x4Components};
  static inline FieldType UInt32{ShaderFieldBaseType::UInt32, 1};
  static inline FieldType Int32{ShaderFieldBaseType::Int32, 1};
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

#endif // GFX_SHADER_TYPES
