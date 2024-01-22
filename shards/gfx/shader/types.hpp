#ifndef C9D0BB09_616F_4DAF_BFFF_4328B7893E6A
#define C9D0BB09_616F_4DAF_BFFF_4328B7893E6A

#include "../enums.hpp"
#include "fwd.hpp"
#include <optional>
#include <string>
#include <variant>
#include <compare>
#include <vector>

namespace gfx {
// TODO: Rename to BaseType
// TODO: Move to shader namespace
enum class ShaderFieldBaseType { Bool, UInt8, Int8, UInt16, Int16, UInt32, Int32, Float16, Float32 };
constexpr bool isFloatType(const ShaderFieldBaseType &type) {
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
    return false;
  }
}
constexpr bool isIntegerType(const ShaderFieldBaseType &type) { return !isFloatType(type); }
constexpr size_t getByteSize(const ShaderFieldBaseType &type) {
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
    return 0;
  }
}
size_t getWGSLAlignment(const ShaderFieldBaseType &type);

namespace shader {

enum class AddressSpace {
  Uniform,
  Storage,
  StorageRW,
};

struct TextureType {
  gfx::TextureDimension dimension = gfx::TextureDimension::D2;
  TextureSampleType format = TextureSampleType::Float;

  TextureType() = default;
  TextureType(gfx::TextureDimension dimension, TextureSampleType format = TextureSampleType::Float)
      : dimension(dimension), format(format) {}

  std::strong_ordering operator<=>(const TextureType &other) const = default;
};

struct SamplerType {
  std::strong_ordering operator<=>(const SamplerType &other) const = default;
};

// Structure to represent wgsl numerical types (numbers, vectors, matrices)
struct NumType {
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

  bool atomic = false;

  NumType() = default;
  NumType(ShaderFieldBaseType baseType) : baseType(baseType), numComponents(1) {}
  NumType(ShaderFieldBaseType baseType, size_t numComponents, size_t matrixDimension = 1)
      : baseType(baseType), numComponents(numComponents), matrixDimension(matrixDimension) {}

  std::strong_ordering operator<=>(const NumType &other) const = default;

  NumType asAtomic() const {
    NumType result = *this;
    result.atomic = true;
    return result;
  }

  size_t getByteSize() const;
  size_t getWGSLAlignment() const;
};

struct StructType;
struct ArrayType;
using Type = std::variant<TextureType, SamplerType, NumType, ArrayType, StructType>;

struct ArrayTypeImpl;
struct ArrayType {
private:
  std::unique_ptr<ArrayTypeImpl> impl; // Indirection because of variant declaration
public:
  ArrayType();
  ArrayType(const Type &elementType, std::optional<size_t> fixedLength = std::nullopt);
  ArrayType(const ArrayType &other);
  ArrayType(ArrayType &&other) = default;
  ArrayType &operator=(const ArrayType &other);

  ArrayTypeImpl *operator->() { return impl.get(); }
  const ArrayTypeImpl *operator->() const { return impl.get(); }
  ArrayTypeImpl &operator*() { return *impl; }
  const ArrayTypeImpl &operator*() const { return *impl; }

  std::strong_ordering operator<=>(const ArrayType &other) const;
  bool operator==(const ArrayType &b) const;
  bool operator!=(const ArrayType &b) const;
};

struct StructTypeImpl;
struct StructType {
private:
  std::unique_ptr<StructTypeImpl> impl; // Indirection because of variant declaration
public:
  StructType();
  StructType(const StructType &other);
  StructType(StructType &&other) = default;
  StructType &operator=(const StructType &other);

  StructTypeImpl *operator->() { return impl.get(); }
  const StructTypeImpl *operator->() const { return impl.get(); }
  StructTypeImpl &operator*() { return *impl; }
  const StructTypeImpl &operator*() const { return *impl; }

  std::strong_ordering operator<=>(const StructType &other) const;
  bool operator==(const StructType &b) const;
  bool operator!=(const StructType &b) const;
};

struct ArrayTypeImpl {
  std::optional<size_t> fixedLength;
  Type elementType;

  std::strong_ordering operator<=>(const ArrayTypeImpl &other) const = default;
};
inline ArrayType::ArrayType() { impl = std::make_unique<ArrayTypeImpl>(); }
inline ArrayType::ArrayType(const Type &elementType, std::optional<size_t> fixedLength) {
  impl = std::make_unique<ArrayTypeImpl>();
  impl->elementType = elementType;
  impl->fixedLength = fixedLength;
}
inline ArrayType::ArrayType(const ArrayType &other) { *this = other; }
inline ArrayType &ArrayType::operator=(const ArrayType &other) {
  impl = std::make_unique<ArrayTypeImpl>(*other.impl);
  return *this;
}
inline std::strong_ordering ArrayType::operator<=>(const ArrayType &other) const { return **this <=> *other; }
inline bool ArrayType::operator==(const ArrayType &b) const { return **this == *b; }
inline bool ArrayType::operator!=(const ArrayType &b) const { return **this != *b; }

struct StructField {
  FastString name;
  Type type;

  StructField() = default;
  StructField(FastString name, const Type &type) : name(name), type(type) {}

  std::strong_ordering operator<=>(const StructField &other) const = default;
};
struct StructTypeImpl {
  std::vector<StructField> entries;

  std::strong_ordering operator<=>(const StructTypeImpl &other) const = default;

  void addField(FastString name, const Type &type) {
    auto existing = std::find_if(entries.begin(), entries.end(), [&](const StructField &f) { return f.name == name; });
    if (existing != entries.end()) {
      if (existing->type == type)
        return;
      throw std::runtime_error("Field already exists with a different type");
    }
    entries.emplace_back(name, type);
  }
};
inline StructType::StructType() { impl = std::make_unique<StructTypeImpl>(); }
inline StructType::StructType(const StructType &other) { *this = other; }
inline StructType &StructType::operator=(const StructType &other) {
  impl = std::make_unique<StructTypeImpl>(*other.impl);
  return *this;
}
inline std::strong_ordering StructType::operator<=>(const StructType &other) const { return **this <=> *other; }
inline bool StructType::operator==(const StructType &b) const { return **this == *b; }
inline bool StructType::operator!=(const StructType &b) const { return **this != *b; }

inline std::strong_ordering operator<=>(const Type &a, const Type &b) {
  auto ci = a.index() <=> b.index();
  if (ci != 0)
    return ci;

  return std::visit(
      [&](auto &arg) {
        using T = std::decay_t<decltype(arg)>;
        return arg <=> std::get<T>(b);
      },
      a);
}
inline std::strong_ordering operator<=>(const Type &a, const NumType &b) { return a <=> Type(b); }
inline bool operator==(const Type &a, const NumType &b) { return a <=> b == std::strong_ordering::equal; }
inline bool operator!=(const Type &a, const NumType &b) { return a <=> b != std::strong_ordering::equal; }

/// <div rustbindgen hide></div>
struct Types {
  static inline NumType Float{ShaderFieldBaseType::Float32};
  static inline NumType Float2{ShaderFieldBaseType::Float32, 2};
  static inline NumType Float3{ShaderFieldBaseType::Float32, 3};
  static inline NumType Float4{ShaderFieldBaseType::Float32, 4};
  static inline NumType Float2x2{ShaderFieldBaseType::Float32, 2, 2};
  static inline NumType Float3x3{ShaderFieldBaseType::Float32, 3, 3};
  static inline NumType Float4x4{ShaderFieldBaseType::Float32, 4, 4};
  static inline NumType UInt32{ShaderFieldBaseType::UInt32, 1};
  static inline NumType Int32{ShaderFieldBaseType::Int32, 1};
  static inline NumType Int2{ShaderFieldBaseType::Int32, 2};
  static inline NumType Int3{ShaderFieldBaseType::Int32, 3};
  static inline NumType Int4{ShaderFieldBaseType::Int32, 4};
  static inline NumType Bool{ShaderFieldBaseType::Bool, 1};
};

struct NamedNumType {
  FastString name;
  NumType type;

  NamedNumType() = default;
  NamedNumType(FastString name, const NumType &type) : name(name), type(type) {}
  NamedNumType(FastString name, NumType &&type) : name(name), type(type) {}
};

} // namespace shader
} // namespace gfx

#endif /* C9D0BB09_616F_4DAF_BFFF_4328B7893E6A */
