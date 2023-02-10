#ifndef C9D0BB09_616F_4DAF_BFFF_4328B7893E6A
#define C9D0BB09_616F_4DAF_BFFF_4328B7893E6A

#include "../enums.hpp"
#include "fwd.hpp"
#include <optional>
#include <string>
#include <variant>
#include <compare>

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
  gfx::TextureDimension dimension = gfx::TextureDimension::D2;
  ShaderTextureFormat format = ShaderTextureFormat::Float32;

  TextureFieldType() = default;
  TextureFieldType(gfx::TextureDimension dimension, ShaderTextureFormat format = ShaderTextureFormat::Float32)
      : dimension(dimension), format(format) {}

  std::strong_ordering operator<=>(const TextureFieldType &other) const = default;
};

struct SamplerFieldType {
  std::strong_ordering operator<=>(const SamplerFieldType &other) const = default;
};

// Structure to represent wgsl numerical types (numbers, vectors, matrices)
struct NumFieldType {
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

  NumFieldType() = default;
  NumFieldType(ShaderFieldBaseType baseType) : baseType(baseType), numComponents(1) {}
  NumFieldType(ShaderFieldBaseType baseType, size_t numComponents, size_t matrixDimension = 1)
      : baseType(baseType), numComponents(numComponents), matrixDimension(matrixDimension) {}

  std::strong_ordering operator<=>(const NumFieldType &other) const = default;

  size_t getByteSize() const;
  size_t getWGSLAlignment() const;
};

using FieldType = std::variant<TextureFieldType, SamplerFieldType, NumFieldType>;

inline std::strong_ordering operator<=>(const FieldType &a, const FieldType &b) {
  auto ci = a.index() <=> b.index();
  if (ci != 0)
    return ci;

  std::visit(
      [&](auto &arg) {
        using T = std::decay_t<decltype(arg)>;
        return arg <=> std::get<T>(b);
      },
      a);
}
inline std::strong_ordering operator<=>(const FieldType &a, const NumFieldType &b) { return a <=> FieldType(b); }
inline bool operator==(const FieldType &a, const NumFieldType &b) { return std::equal_to<FieldType>{}(a, b); }
inline bool operator!=(const FieldType &a, const NumFieldType &b) { return !std::equal_to<FieldType>{}(a, b); }

/// <div rustbindgen hide></div>
struct FieldTypes {
  static inline NumFieldType Float{ShaderFieldBaseType::Float32};
  static inline NumFieldType Float2{ShaderFieldBaseType::Float32, 2};
  static inline NumFieldType Float3{ShaderFieldBaseType::Float32, 3};
  static inline NumFieldType Float4{ShaderFieldBaseType::Float32, 4};
  static inline NumFieldType Float2x2{ShaderFieldBaseType::Float32, 2, 2};
  static inline NumFieldType Float3x3{ShaderFieldBaseType::Float32, 3, 3};
  static inline NumFieldType Float4x4{ShaderFieldBaseType::Float32, 4, 4};
  static inline NumFieldType UInt32{ShaderFieldBaseType::UInt32, 1};
  static inline NumFieldType Int32{ShaderFieldBaseType::Int32, 1};
  static inline NumFieldType Bool{ShaderFieldBaseType::Bool, 1};
};

struct NamedField {
  String name;
  NumFieldType type;

  NamedField() = default;
  NamedField(String name, const NumFieldType &type) : name(name), type(type) {}
  NamedField(String name, NumFieldType &&type) : name(name), type(type) {}
};

} // namespace shader
} // namespace gfx

#endif /* C9D0BB09_616F_4DAF_BFFF_4328B7893E6A */
