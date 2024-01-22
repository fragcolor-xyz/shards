#ifndef E8607873_067B_4B77_B01B_AC8DF6D5A64D
#define E8607873_067B_4B77_B01B_AC8DF6D5A64D

#include "struct_layout.hpp"
#include "layout_traverser2.hpp"
#include "fmt.hpp"
namespace gfx::shader {

namespace detail {
template <typename T, typename V = void> struct BufferSerializable {
  using Valid = std::false_type;
};
template <typename T, typename V = void> struct CompatibleBaseType {
  using Valid = std::false_type;
};
template <typename T> struct CompatibleBaseType<T, std::enable_if_t<std::is_integral_v<T>>> {
  using Valid = std::true_type;
  static constexpr bool isCompatible(ShaderFieldBaseType bt) { return isIntegerType(bt); }
  template <ShaderFieldBaseType BaseType> static inline void write(uint8_t *dst, const T &src) {
    if constexpr (BaseType == ShaderFieldBaseType::Int32) {
      *reinterpret_cast<int32_t *>(dst) = (int32_t)src;
    } else if constexpr (BaseType == ShaderFieldBaseType::UInt32) {
      *reinterpret_cast<uint32_t *>(dst) = (uint32_t)src;
    }
  }
  template <ShaderFieldBaseType BaseType> static inline void read(T &dst, const uint8_t *data) {
    if constexpr (BaseType == ShaderFieldBaseType::Int32) {
      dst = (T) * reinterpret_cast<const int32_t *>(data);
    } else if constexpr (BaseType == ShaderFieldBaseType::UInt32) {
      dst = (T) * reinterpret_cast<const uint32_t *>(data);
    }
  }
};
template <typename T> struct CompatibleBaseType<T, std::enable_if_t<std::is_floating_point_v<T>>> {
  using Valid = std::true_type;
  static constexpr bool isCompatible(ShaderFieldBaseType bt) { return isFloatType(bt); }
  template <ShaderFieldBaseType Type> static inline void write(uint8_t *dst, const T &src) {
    if constexpr (Type == ShaderFieldBaseType::Float32) {
      *reinterpret_cast<float *>(dst) = src;
    } else if constexpr (Type == ShaderFieldBaseType::Float16) {
      throw std::logic_error("Half float not implemented");
    }
  }
  template <ShaderFieldBaseType Type> static inline void read(T &dst, const uint8_t *data) {
    if constexpr (Type == ShaderFieldBaseType::Float32) {
      dst = *reinterpret_cast<const float *>(data);
    } else if constexpr (Type == ShaderFieldBaseType::Float16) {
      throw std::logic_error("Half float not implemented");
    }
  }
};
template <typename T> struct BufferSerializable<T, std::enable_if_t<CompatibleBaseType<T>::Valid::value>> {
  using Self = T;
  using Valid = std::true_type;
  using CT = CompatibleBaseType<T>;

  static constexpr bool canConvertTo(const NumType &nt) {
    if (nt.matrixDimension > 1 || nt.numComponents > 1)
      return false;
    return CT::isCompatible(nt.baseType);
  }
  template <ShaderFieldBaseType BaseType> void write(const NumType &nt, uint8_t *dst, const Self &src) {
    CT::template write<BaseType>(dst, src);
  }
  template <ShaderFieldBaseType BaseType> void read(const NumType &nt, Self &dst, const uint8_t *src) {
    CT::template read<BaseType>(dst, src);
  }
};

template <typename T, int M> struct BufferSerializable<linalg::vec<T, M>, std::enable_if_t<CompatibleBaseType<T>::Valid::value>> {
  using Self = linalg::vec<T, M>;
  using Valid = std::true_type;
  using CT = CompatibleBaseType<T>;

  static constexpr bool canConvertTo(const NumType &nt) {
    if (!CT::isCompatible(nt.baseType)) {
      return false;
    }
    return nt.matrixDimension == 1 && nt.numComponents == M;
  }

  template <ShaderFieldBaseType BaseType> void write(const NumType &nt, uint8_t *dst, const Self &src) {
    for (int i = 0; i < M; i++) {
      CT::template write<BaseType>(dst + getByteSize(BaseType) * i, src[i]);
    }
  }

  template <ShaderFieldBaseType BaseType> void read(const NumType &nt, Self &dst, const uint8_t *src) {
    for (int i = 0; i < M; i++) {
      CT::template read<BaseType>(dst[i], src + getByteSize(BaseType) * i);
    }
  }
};

template <typename S, typename T> struct CallWrite {
  S s;
  const NumType &nt;
  uint8_t *dst;
  const T &src;

  template <ShaderFieldBaseType BaseType> void visit() { s.template write<BaseType>(nt, dst, src); }
};

template <typename S, typename T> struct CallRead {
  S s;
  const NumType &nt;
  T &dst;
  const uint8_t *src;

  template <ShaderFieldBaseType BaseType> void visit() { s.template read<BaseType>(nt, dst, src); }
};
} // namespace detail

struct SerializeTypeMismatch {
  Type expected;
};

struct BufferSerializer {
private:
  uint8_t *data{};
  size_t length{};

public:
  BufferSerializer(uint8_t *data, size_t length) : data(data), length(length) {}
  BufferSerializer(std::vector<uint8_t> &v) : data(v.data()), length(v.size()) {}

  template <typename T> constexpr void visitHostSerializableBaseType(ShaderFieldBaseType bt, T target) {
    switch (bt) {
    case ShaderFieldBaseType::Float32:
      target.template visit<ShaderFieldBaseType::Float32>();
      break;
    case ShaderFieldBaseType::Int32:
      target.template visit<ShaderFieldBaseType::Int32>();
      break;
    case ShaderFieldBaseType::UInt32:
      target.template visit<ShaderFieldBaseType::UInt32>();
      break;
    default:
      throw std::logic_error("Not a host shareable type");
    }
  }

  template <typename T> void write(const LayoutRef2 &ref, const T &src) { write(ref.type(), ref.offset, src); }

  template <typename T> void read(const LayoutRef2 &ref, T &dst) { read(ref.type(), ref.offset, dst); }

  template <typename T> void write(const LayoutRef &ref, const T &src) { write(ref.type, ref.offset, src); }

  template <typename T> void read(const LayoutRef &ref, T &dst) { read(ref.type, ref.offset, dst); }

  template <typename T> std::enable_if_t<detail::BufferSerializable<T>::Valid::value> write(const Type &type, size_t offset, const T &src) {
    using S = detail::BufferSerializable<T>;
    static_assert(S::Valid::value, "Type is not buffer serializable");

    auto numType = std::get_if<NumType>(&type);
    if (!numType) {
      throw std::runtime_error("Target is not a number type");
    }
    if (!S::canConvertTo(*numType)) {
      throw SerializeTypeMismatch{.expected = type};
    }

    size_t size = numType->getByteSize();
    if (offset + size > length)
      throw std::runtime_error(fmt::format("Write out of bounds at offset {} (max: {})", offset, length));

    visitHostSerializableBaseType(numType->baseType, detail::CallWrite<S, T>{
                                                         .nt = *numType,
                                                         .dst = data + offset,
                                                         .src = src,
                                                     });
  }

  template <typename T> std::enable_if_t<detail::BufferSerializable<T>::Valid::value> read(const Type &type, size_t offset, T &dst) {
    using S = detail::BufferSerializable<T>;
    static_assert(S::Valid::value, "Type is not buffer serializable");

    auto numType = std::get_if<NumType>(&type);
    if (!numType) {
      throw std::runtime_error("Target is not a number type");
    }
    if (!S::canConvertTo(*numType)) {
      throw SerializeTypeMismatch{.expected = type};
    }

    size_t size = numType->getByteSize();
    if (offset + size > length)
      throw std::runtime_error(fmt::format("Read out of bounds at offset {} (max: {})", offset, length));

    visitHostSerializableBaseType(numType->baseType, detail::CallRead<S, T>{
                                                         .nt = *numType,
                                                         .dst = dst,
                                                         .src = data + offset,
                                                     });
  }
};
} // namespace gfx::shader

#endif /* E8607873_067B_4B77_B01B_AC8DF6D5A64D */
