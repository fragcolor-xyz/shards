#ifndef C0C58AE8_8145_4443_B0B5_9D739E8256FE
#define C0C58AE8_8145_4443_B0B5_9D739E8256FE

#include "magic_enum.hpp"
#include "shards.h"
#include <stdexcept>
#include <spdlog/fmt/fmt.h>
#include <magic_enum.hpp>

namespace shards::Math {

template <size_t Length> struct ApplyUnaryVector {
  template <typename TOp, typename T, typename... TArgs> void apply(T &out, const T &a, TArgs... args) {
    TOp op{};

    for (size_t i = 0; i < Length; i++)
      out[i] = op.template apply<>(a[i], args...);
  }
};

template <> struct ApplyUnaryVector<1> {
  template <typename TOp, typename T, typename... TArgs> void apply(T &out, const T &a, TArgs... args) {
    TOp op{};
    out = op.template apply<>(a, args...);
  }
};

struct ApplyUnaryColor {
  template <typename TOp, typename T, typename... TArgs> void apply(T &out, const T &a, TArgs... args) {
    TOp op{};
    out.r = op.template apply<>(a.r, args...);
    out.g = op.template apply<>(a.g, args...);
    out.b = op.template apply<>(a.b, args...);
    out.a = op.template apply<>(a.a, args...);
  }
};

template <size_t Length> struct ApplyBinaryVector {
  template <typename TOp, typename T, typename... TArgs> void apply(T &out, const T &a, const T &b, TArgs... args) {
    TOp op{};

    for (size_t i = 0; i < Length; i++)
      out[i] = op.template apply<>(a[i], b[i], args...);
  }
};

template <> struct ApplyBinaryVector<1> {
  template <typename TOp, typename T, typename... TArgs> void apply(T &out, const T &a, const T &b, TArgs... args) {
    TOp op{};
    out = op.template apply<>(a, b, args...);
  }
};

struct ApplyBinaryColor {
  template <typename TOp, typename T, typename... TArgs> void apply(T &out, const T &a, const T &b, TArgs... args) {
    TOp op{};
    out.r = op.template apply<>(a.r, b.r, args...);
    out.g = op.template apply<>(a.g, b.g, args...);
    out.b = op.template apply<>(a.b, b.b, args...);
    out.a = op.template apply<>(a.a, b.a, args...);
  }
};

template <size_t Length> struct BroadcastVector {
  template <typename T, typename T1> void apply(T &out, const T1 &a) {
    for (size_t i = 0; i < Length; i++)
      out[i] = a;
  }
};

template <> struct BroadcastVector<1> {
  template <typename T, typename T1> void apply(T &out, const T1 &a) { out = a; };
};

struct BroadcastColor {
  template <typename T, typename T1> void apply(T &out, const T1 &a) {
    out.r = a;
    out.g = a;
    out.b = a;
    out.a = a;
  };
};

template <SHType BasicType> struct PayloadTraits {};

template <> struct PayloadTraits<SHType::Bool> {
  using Type = SHInt;
  using ApplyBinary = ApplyBinaryVector<1>;
  using ApplyUnary = ApplyUnaryVector<1>;
  using Broadcast = BroadcastVector<1>;
  using UnitType = PayloadTraits<SHType::Bool>;
  auto &getContents(SHVarPayload &payload) { return payload.boolValue; }
};
template <> struct PayloadTraits<SHType::Float> {
  using Type = SHFloat;
  using ApplyBinary = ApplyBinaryVector<1>;
  using ApplyUnary = ApplyUnaryVector<1>;
  using Broadcast = BroadcastVector<1>;
  using UnitType = PayloadTraits<SHType::Float>;
  auto &getContents(SHVarPayload &payload) { return payload.floatValue; }
};
template <> struct PayloadTraits<SHType::Float2> {
  using Type = SHFloat2;
  using ApplyBinary = ApplyBinaryVector<2>;
  using ApplyUnary = ApplyUnaryVector<2>;
  using Broadcast = BroadcastVector<2>;
  using UnitType = PayloadTraits<SHType::Float>;
  auto &getContents(SHVarPayload &payload) { return payload.float2Value; }
};
template <> struct PayloadTraits<SHType::Float3> {
  using Type = SHFloat3;
  using ApplyBinary = ApplyBinaryVector<3>;
  using ApplyUnary = ApplyUnaryVector<3>;
  using Broadcast = BroadcastVector<3>;
  using UnitType = PayloadTraits<SHType::Float>;
  auto &getContents(SHVarPayload &payload) { return payload.float3Value; }
};
template <> struct PayloadTraits<SHType::Float4> {
  using Type = SHFloat4;
  using ApplyBinary = ApplyBinaryVector<4>;
  using ApplyUnary = ApplyUnaryVector<4>;
  using Broadcast = BroadcastVector<4>;
  using UnitType = PayloadTraits<SHType::Float>;
  auto &getContents(SHVarPayload &payload) { return payload.float4Value; }
};
template <> struct PayloadTraits<SHType::Int> {
  using Type = SHInt;
  using ApplyBinary = ApplyBinaryVector<1>;
  using ApplyUnary = ApplyUnaryVector<1>;
  using Broadcast = BroadcastVector<1>;
  using UnitType = PayloadTraits<SHType::Int>;
  auto &getContents(SHVarPayload &payload) { return payload.intValue; }
};
template <> struct PayloadTraits<SHType::Int2> {
  using Type = SHInt2;
  using ApplyBinary = ApplyBinaryVector<2>;
  using ApplyUnary = ApplyUnaryVector<2>;
  using Broadcast = BroadcastVector<2>;
  using UnitType = PayloadTraits<SHType::Int>;
  auto &getContents(SHVarPayload &payload) { return payload.int2Value; }
};
template <> struct PayloadTraits<SHType::Int3> {
  using Type = SHInt3;
  using ApplyBinary = ApplyBinaryVector<3>;
  using ApplyUnary = ApplyUnaryVector<3>;
  using Broadcast = BroadcastVector<3>;
  using UnitType = PayloadTraits<SHType::Int>;
  auto &getContents(SHVarPayload &payload) { return payload.int3Value; }
};
template <> struct PayloadTraits<SHType::Int4> {
  using Type = SHInt4;
  using ApplyBinary = ApplyBinaryVector<4>;
  using ApplyUnary = ApplyUnaryVector<4>;
  using Broadcast = BroadcastVector<4>;
  using UnitType = PayloadTraits<SHType::Int>;
  auto &getContents(SHVarPayload &payload) { return payload.int4Value; }
};
template <> struct PayloadTraits<SHType::Int8> {
  using Type = SHInt8;
  using ApplyBinary = ApplyBinaryVector<8>;
  using ApplyUnary = ApplyUnaryVector<8>;
  using Broadcast = BroadcastVector<8>;
  using UnitType = PayloadTraits<SHType::Int>;
  auto &getContents(SHVarPayload &payload) { return payload.int8Value; }
};
template <> struct PayloadTraits<SHType::Int16> {
  using Type = SHInt16;
  using ApplyBinary = ApplyBinaryVector<16>;
  using ApplyUnary = ApplyUnaryVector<16>;
  using Broadcast = BroadcastVector<16>;
  using UnitType = PayloadTraits<SHType::Int>;
  auto &getContents(SHVarPayload &payload) { return payload.int16Value; }
};
template <> struct PayloadTraits<SHType::Color> {
  using Type = SHColor;
  using ApplyBinary = ApplyBinaryColor;
  using ApplyUnary = ApplyUnaryColor;
  using Broadcast = BroadcastColor;
  using UnitType = PayloadTraits<SHType::Int>;
  auto &getContents(SHVarPayload &payload) { return payload.colorValue; }
};

template <SHType ValueType> auto &getPayloadContents(SHVarPayload &payload) {
  PayloadTraits<ValueType> traits;
  return traits.getContents(payload);
}

template <SHType ValueType> const auto &getPayloadContents(const SHVarPayload &payload) {
  PayloadTraits<ValueType> traits;
  return traits.getContents(const_cast<SHVarPayload &>(payload));
}

enum class DispatchType : uint8_t {
  FloatTypes = 0x1,
  IntTypes = 0x2,
  BoolTypes = 0x4,
  NumberTypes = FloatTypes | IntTypes,
  IntOrBoolTypes = IntTypes | BoolTypes,
};
} // namespace shards::Math

namespace magic_enum::customize {
template <> struct enum_range<shards::Math::DispatchType> {
  static constexpr bool is_flags = true;
};
} // namespace magic_enum::customize

namespace shards::Math {
constexpr bool hasDispatchType(DispatchType a, DispatchType b) { return (uint8_t(a) & uint8_t(b)) != 0; }

template <DispatchType DispatchType, typename T, typename... TArgs> void dispatchType(SHType type, T v, TArgs &&...args) {
  switch (type) {
  case SHType::Int:
    if constexpr (hasDispatchType(DispatchType, DispatchType::IntTypes))
      return v.template apply<SHType::Int>(std::forward<TArgs>(args)...);
    break;
  case SHType::Int2:
    if constexpr (hasDispatchType(DispatchType, DispatchType::IntTypes))
      return v.template apply<SHType::Int2>(std::forward<TArgs>(args)...);
    break;
  case SHType::Int3:
    if constexpr (hasDispatchType(DispatchType, DispatchType::IntTypes))
      return v.template apply<SHType::Int3>(std::forward<TArgs>(args)...);
    break;
  case SHType::Int4:
    if constexpr (hasDispatchType(DispatchType, DispatchType::IntTypes))
      return v.template apply<SHType::Int4>(std::forward<TArgs>(args)...);
    break;
  case SHType::Int8:
    if constexpr (hasDispatchType(DispatchType, DispatchType::IntTypes))
      return v.template apply<SHType::Int8>(std::forward<TArgs>(args)...);
    break;
  case SHType::Int16:
    if constexpr (hasDispatchType(DispatchType, DispatchType::IntTypes))
      return v.template apply<SHType::Int16>(std::forward<TArgs>(args)...);
    break;
  case SHType::Color:
    if constexpr (hasDispatchType(DispatchType, DispatchType::IntTypes))
      return v.template apply<SHType::Color>(std::forward<TArgs>(args)...);
    break;
  case SHType::Float:
    if constexpr (hasDispatchType(DispatchType, DispatchType::FloatTypes))
      return v.template apply<SHType::Float>(std::forward<TArgs>(args)...);
    break;
  case SHType::Float2:
    if constexpr (hasDispatchType(DispatchType, DispatchType::FloatTypes))
      return v.template apply<SHType::Float2>(std::forward<TArgs>(args)...);
    break;
  case SHType::Float3:
    if constexpr (hasDispatchType(DispatchType, DispatchType::FloatTypes))
      return v.template apply<SHType::Float3>(std::forward<TArgs>(args)...);
    break;
  case SHType::Float4:
    if constexpr (hasDispatchType(DispatchType, DispatchType::FloatTypes))
      return v.template apply<SHType::Float4>(std::forward<TArgs>(args)...);
    break;
  case SHType::Bool:
    if constexpr (hasDispatchType(DispatchType, DispatchType::BoolTypes))
      return v.template apply<SHType::Bool>(std::forward<TArgs>(args)...);
    break;
  default:
    break;
  }
  throw std::out_of_range(
      fmt::format("dispatchType<{}>({})", magic_enum::enum_flags_name(DispatchType), magic_enum::enum_name(type)));
}

struct ModOp final {
  template <typename T> T apply(const T &lhs, const T &rhs) {
    if constexpr (std::is_floating_point<T>::value) {
      // Use std::fmod for floating-point types
      return std::fmod(lhs, rhs);
    } else {
      // Use the modulo operator for integral types
      return lhs % rhs;
    }
  }
};

#define MATH_BINARY_OPERATION(__name, __op)                                            \
  struct __name##Op final {                                                            \
    template <typename T> T apply(const T &lhs, const T &rhs) { return lhs __op rhs; } \
  };

MATH_BINARY_OPERATION(Add, +);
MATH_BINARY_OPERATION(Subtract, -);
MATH_BINARY_OPERATION(Multiply, *);
MATH_BINARY_OPERATION(Divide, /);
MATH_BINARY_OPERATION(Xor, ^);
MATH_BINARY_OPERATION(And, &);
MATH_BINARY_OPERATION(Or, |);
MATH_BINARY_OPERATION(LShift, <<);
MATH_BINARY_OPERATION(RShift, >>);

#undef MATH_BINARY_OPERATION

} // namespace shards::Math

#endif /* C0C58AE8_8145_4443_B0B5_9D739E8256FE */
