/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2019 Fragcolor Pte. Ltd. */

#ifndef CB_CHAINBLOCKS_HPP
#define CB_CHAINBLOCKS_HPP

#include "chainblocks.h"
#include <cstring> // memcpy
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace chainblocks {
constexpr uint32_t CoreCC = 'sink'; // 1936289387

class CBException : public std::exception {
public:
  explicit CBException(std::string_view msg) : errorMessage(msg) {}

  [[nodiscard]] const char *what() const noexcept override {
    return errorMessage.data();
  }

private:
  std::string errorMessage;
};

class InvalidParameterIndex : public CBException {
public:
  explicit InvalidParameterIndex() : CBException("Invalid parameter index") {}
};

class ActivationError : public CBException {
public:
  explicit ActivationError(std::string_view msg) : CBException(msg) {}
};

class WarmupError : public CBException {
public:
  explicit WarmupError(std::string_view msg) : CBException(msg) {}
};

class ComposeError : public CBException {
public:
  explicit ComposeError(std::string_view msg, bool fatal = true)
      : CBException(msg), fatal(fatal) {}

  bool triggerFailure() const { return fatal; }

private:
  bool fatal;
};

class InvalidVarTypeError : public CBException {
public:
  explicit InvalidVarTypeError(std::string_view msg) : CBException(msg) {}
};

CBlock *createBlock(std::string_view name);

struct Type {
  Type() : _type({CBType::None}) {}

  Type(CBTypeInfo type) : _type(type) {}

  Type &operator=(Type other) {
    _type = other._type;
    return *this;
  }

  Type &operator=(CBTypeInfo other) {
    _type = other;
    return *this;
  }

  operator CBTypesInfo() {
    CBTypesInfo res{&_type, 1, 0};
    return res;
  }

  operator CBTypeInfo() { return _type; }

  static Type SeqOf(CBTypesInfo types) {
    Type res;
    res._type = {CBType::Seq, {.seqTypes = types}};
    return res;
  }

  static Type SeqOf(CBTypesInfo types, uint32_t fixedLen) {
    Type res;
    res._type = {CBType::Seq, {.seqTypes = types}, fixedLen};
    return res;
  }

  static Type VariableOf(CBTypesInfo types) {
    Type res;
    res._type = {CBType::ContextVar, {.contextVarTypes = types}};
    return res;
  }

  static Type Enum(int32_t vendorId, int32_t typeId) {
    Type res;
    res._type = {CBType::Enum,
                 {.enumeration = {.vendorId = vendorId, .typeId = typeId}}};
    return res;
  }

  static Type TableOf(CBTypesInfo types) {
    Type res;
    res._type = {CBType::Table, {.table = {.types = types}}};
    return res;
  }

  template <size_t N>
  static Type TableOf(CBTypesInfo types, const std::array<CBString, N> &keys) {
    Type res;
    auto &k = const_cast<std::array<CBString, N> &>(keys);
    res._type = {
        CBType::Table,
        {.table = {.keys = {&k[0], uint32_t(k.size()), 0}, .types = types}}};
    return res;
  }

private:
  CBTypeInfo _type;
};

struct Types {
  std::vector<CBTypeInfo> _types;

  Types() {}

  Types(std::initializer_list<CBTypeInfo> types, bool addSelf = false)
      : _types(types) {
    if (addSelf) {
      auto idx = _types.size();
      // make sure the memory will stay valid
      _types.resize(idx + 1);
      // add self
      _types[idx] = Type::SeqOf(CBTypesInfo(*this));
      _types[idx].recursiveSelf = true;
    }
  }

  Types(const Types &others, std::initializer_list<CBTypeInfo> types,
        bool addSelf = false) {
    for (auto &type : others._types) {
      _types.push_back(type);
    }
    for (auto &type : types) {
      _types.push_back(type);
    }
    if (addSelf) {
      auto idx = _types.size();
      // make sure the memory will stay valid
      _types.resize(idx + 1);
      // add self
      _types[idx] = Type::SeqOf(CBTypesInfo(*this));
      _types[idx].recursiveSelf = true;
    }
  }

  Types(const std::vector<CBTypeInfo> &types) { _types = types; }

  Types &operator=(const std::vector<CBTypeInfo> &types) {
    _types = types;
    return *this;
  }

  operator CBTypesInfo() {
    CBTypesInfo res{&_types[0], (uint32_t)_types.size(), 0};
    return res;
  }
};

struct ParameterInfo {
  CBString _name;
  CBOptionalString _help;
  Types _types;

  ParameterInfo(CBString name, CBOptionalString help, Types types)
      : _name(name), _help(help), _types(types) {}
  ParameterInfo(CBString name, Types types)
      : _name(name), _help({}), _types(types) {}

  operator CBParameterInfo() {
    CBParameterInfo res{_name, _help, _types};
    return res;
  }
};

struct Parameters {
  std::vector<ParameterInfo> _infos{};
  std::vector<CBParameterInfo> _pinfos{};

  Parameters() = default;

  Parameters(const Parameters &others,
             const std::vector<ParameterInfo> &infos) {
    for (auto &info : others._infos) {
      _infos.push_back(info);
    }
    for (auto &info : infos) {
      _infos.push_back(info);
    }
    // update the C type cache
    for (auto &info : _infos) {
      _pinfos.push_back(info);
    }
  }

  // THE CONSTRUCTORS UNDER ARE UNSAFE
  // static inline is nice but it's likely unordered!
  // won't likely work with some compilers
  // and if linking dlls!

  Parameters(const Parameters &others,
             std::initializer_list<ParameterInfo> infos) {
    for (auto &info : others._infos) {
      _infos.push_back(info);
    }
    for (auto &info : infos) {
      _infos.push_back(info);
    }
    // update the C type cache
    for (auto &info : _infos) {
      _pinfos.push_back(info);
    }
  }

  Parameters(std::initializer_list<ParameterInfo> infos,
             const Parameters &others) {
    for (auto &info : infos) {
      _infos.push_back(info);
    }
    for (auto &info : others._infos) {
      _infos.push_back(info);
    }
    // update the C type cache
    for (auto &info : _infos) {
      _pinfos.push_back(info);
    }
  }

  Parameters(std::initializer_list<ParameterInfo> infos) : _infos(infos) {
    for (auto &info : _infos) {
      _pinfos.push_back(info);
    }
  }

  operator CBParametersInfo() {
    if (_pinfos.empty())
      return CBParametersInfo{nullptr, 0, 0};
    else
      return CBParametersInfo{&_pinfos.front(), (uint32_t)_pinfos.size(), 0};
  }
};

// used to explicitly specialize, hinting compiler
// mostly used internally for math blocks
#define CB_PAYLOAD_CTORS0(CBPAYLOAD_TYPE, __inner_type__, __item__)            \
  constexpr static size_t Width{1};                                            \
  CBPAYLOAD_TYPE() : CBVarPayload() {}                                         \
  template <typename NUMBER> CBPAYLOAD_TYPE(NUMBER n) : CBVarPayload() {       \
    this->__item__ = __inner_type__(n);                                        \
  }                                                                            \
  template <typename NUMBER>                                                   \
  CBPAYLOAD_TYPE(std::initializer_list<NUMBER> l) : CBVarPayload() {           \
    const NUMBER *p = l.begin();                                               \
    this->__item__ = __inner_type__(p[0]);                                     \
  }

#define CB_PAYLOAD_CTORS1(CBPAYLOAD_TYPE, __inner_type__, __item__, _width_)   \
  constexpr static size_t Width{_width_};                                      \
  CBPAYLOAD_TYPE() : CBVarPayload() {}                                         \
  template <typename NUMBER> CBPAYLOAD_TYPE(NUMBER n) : CBVarPayload() {       \
    for (size_t i = 0; i < _width_; ++i)                                       \
      this->__item__[i] = __inner_type__(n);                                   \
  }                                                                            \
  template <typename NUMBER>                                                   \
  CBPAYLOAD_TYPE(std::initializer_list<NUMBER> l) : CBVarPayload() {           \
    const NUMBER *p = l.begin();                                               \
    for (size_t i = 0; i < l.size(); ++i) {                                    \
      this->__item__[i] = __inner_type__(p[i]);                                \
    }                                                                          \
  }

#define CB_PAYLOAD_MATH_OPS(CBPAYLOAD_TYPE, __item__)                          \
  ALWAYS_INLINE inline CBPAYLOAD_TYPE operator+(const CBPAYLOAD_TYPE &b)       \
      const {                                                                  \
    CBPAYLOAD_TYPE res;                                                        \
    res.__item__ = __item__ + b.__item__;                                      \
    return res;                                                                \
  }                                                                            \
  ALWAYS_INLINE inline CBPAYLOAD_TYPE operator-(const CBPAYLOAD_TYPE &b)       \
      const {                                                                  \
    CBPAYLOAD_TYPE res;                                                        \
    res.__item__ = __item__ - b.__item__;                                      \
    return res;                                                                \
  }                                                                            \
  ALWAYS_INLINE inline CBPAYLOAD_TYPE operator*(const CBPAYLOAD_TYPE &b)       \
      const {                                                                  \
    CBPAYLOAD_TYPE res;                                                        \
    res.__item__ = __item__ * b.__item__;                                      \
    return res;                                                                \
  }                                                                            \
  ALWAYS_INLINE inline CBPAYLOAD_TYPE operator/(const CBPAYLOAD_TYPE &b)       \
      const {                                                                  \
    CBPAYLOAD_TYPE res;                                                        \
    res.__item__ = __item__ / b.__item__;                                      \
    return res;                                                                \
  }                                                                            \
  ALWAYS_INLINE inline CBPAYLOAD_TYPE &operator+=(const CBPAYLOAD_TYPE &b) {   \
    __item__ += b.__item__;                                                    \
    return *this;                                                              \
  }                                                                            \
  ALWAYS_INLINE inline CBPAYLOAD_TYPE &operator-=(const CBPAYLOAD_TYPE &b) {   \
    __item__ -= b.__item__;                                                    \
    return *this;                                                              \
  }                                                                            \
  ALWAYS_INLINE inline CBPAYLOAD_TYPE &operator*=(const CBPAYLOAD_TYPE &b) {   \
    __item__ *= b.__item__;                                                    \
    return *this;                                                              \
  }                                                                            \
  ALWAYS_INLINE inline CBPAYLOAD_TYPE &operator/=(const CBPAYLOAD_TYPE &b) {   \
    __item__ /= b.__item__;                                                    \
    return *this;                                                              \
  }

#define CB_PAYLOAD_MATH_OPS_INT(CBPAYLOAD_TYPE, __item__)                      \
  ALWAYS_INLINE inline CBPAYLOAD_TYPE operator^(const CBPAYLOAD_TYPE &b)       \
      const {                                                                  \
    CBPAYLOAD_TYPE res;                                                        \
    res.__item__ = __item__ ^ b.__item__;                                      \
    return res;                                                                \
  }                                                                            \
  ALWAYS_INLINE inline CBPAYLOAD_TYPE operator&(const CBPAYLOAD_TYPE &b)       \
      const {                                                                  \
    CBPAYLOAD_TYPE res;                                                        \
    res.__item__ = __item__ & b.__item__;                                      \
    return res;                                                                \
  }                                                                            \
  ALWAYS_INLINE inline CBPAYLOAD_TYPE operator|(const CBPAYLOAD_TYPE &b)       \
      const {                                                                  \
    CBPAYLOAD_TYPE res;                                                        \
    res.__item__ = __item__ | b.__item__;                                      \
    return res;                                                                \
  }                                                                            \
  ALWAYS_INLINE inline CBPAYLOAD_TYPE operator%(const CBPAYLOAD_TYPE &b)       \
      const {                                                                  \
    CBPAYLOAD_TYPE res;                                                        \
    res.__item__ = __item__ % b.__item__;                                      \
    return res;                                                                \
  }                                                                            \
  ALWAYS_INLINE inline CBPAYLOAD_TYPE operator<<(const CBPAYLOAD_TYPE &b)      \
      const {                                                                  \
    CBPAYLOAD_TYPE res;                                                        \
    res.__item__ = __item__ << b.__item__;                                     \
    return res;                                                                \
  }                                                                            \
  ALWAYS_INLINE inline CBPAYLOAD_TYPE operator>>(const CBPAYLOAD_TYPE &b)      \
      const {                                                                  \
    CBPAYLOAD_TYPE res;                                                        \
    res.__item__ = __item__ >> b.__item__;                                     \
    return res;                                                                \
  }

#define CB_PAYLOAD_MATH_OP_FLOAT(CBPAYLOAD_TYPE, __item__, __op__)             \
  ALWAYS_INLINE static inline CBPAYLOAD_TYPE __op__(const CBPAYLOAD_TYPE &x) { \
    CBPAYLOAD_TYPE res;                                                        \
    for (size_t i = 0; i < CBPAYLOAD_TYPE::Width; i++) {                       \
      res.__item__[i] = __builtin_##__op__(x.__item__[i]);                     \
    }                                                                          \
    return res;                                                                \
  }

#define CB_PAYLOAD_MATH_OPS_FLOAT(CBPAYLOAD_TYPE, __item__)                    \
  CB_PAYLOAD_MATH_OP_FLOAT(CBPAYLOAD_TYPE, __item__, sqrt);                    \
  CB_PAYLOAD_MATH_OP_FLOAT(CBPAYLOAD_TYPE, __item__, log);                     \
  CB_PAYLOAD_MATH_OP_FLOAT(CBPAYLOAD_TYPE, __item__, sin);                     \
  CB_PAYLOAD_MATH_OP_FLOAT(CBPAYLOAD_TYPE, __item__, cos);                     \
  CB_PAYLOAD_MATH_OP_FLOAT(CBPAYLOAD_TYPE, __item__, exp);                     \
  CB_PAYLOAD_MATH_OP_FLOAT(CBPAYLOAD_TYPE, __item__, tanh);                    \
  CB_PAYLOAD_MATH_OP_FLOAT(CBPAYLOAD_TYPE, __item__, fabs);

#define CB_PAYLOAD_MATH_POW_FLOAT(CBPAYLOAD_TYPE, __item__)                    \
  template <typename P>                                                        \
  ALWAYS_INLINE static inline CBPAYLOAD_TYPE pow(const CBPAYLOAD_TYPE &x,      \
                                                 P p) {                        \
    CBPAYLOAD_TYPE res;                                                        \
    for (size_t i = 0; i < CBPAYLOAD_TYPE::Width; i++) {                       \
      res.__item__[i] = __builtin_pow(x.__item__[i], p);                       \
    }                                                                          \
    return res;                                                                \
  }

#define CB_PAYLOAD_MATH_OPS_SIMPLE(CBPAYLOAD_TYPE, __item__)                   \
  ALWAYS_INLINE inline bool operator<=(const CBPAYLOAD_TYPE &b) const {        \
    return __item__ <= b.__item__;                                             \
  }                                                                            \
  ALWAYS_INLINE inline bool operator>=(const CBPAYLOAD_TYPE &b) const {        \
    return __item__ >= b.__item__;                                             \
  }                                                                            \
  ALWAYS_INLINE inline bool operator==(const CBPAYLOAD_TYPE &b) const {        \
    return __item__ == b.__item__;                                             \
  }                                                                            \
  ALWAYS_INLINE inline bool operator!=(const CBPAYLOAD_TYPE &b) const {        \
    return __item__ != b.__item__;                                             \
  }                                                                            \
  ALWAYS_INLINE inline bool operator>(const CBPAYLOAD_TYPE &b) const {         \
    return __item__ > b.__item__;                                              \
  }                                                                            \
  ALWAYS_INLINE inline bool operator<(const CBPAYLOAD_TYPE &b) const {         \
    return __item__ < b.__item__;                                              \
  }

struct IntVarPayload : public CBVarPayload {
  CB_PAYLOAD_CTORS0(IntVarPayload, int64_t, intValue);
  CB_PAYLOAD_MATH_OPS(IntVarPayload, intValue);
  CB_PAYLOAD_MATH_OPS_SIMPLE(IntVarPayload, intValue);
  CB_PAYLOAD_MATH_OPS_INT(IntVarPayload, intValue);
};
struct Int2VarPayload : public CBVarPayload {
  CB_PAYLOAD_CTORS1(Int2VarPayload, int64_t, int2Value, 2);
  CB_PAYLOAD_MATH_OPS(Int2VarPayload, int2Value);
  CB_PAYLOAD_MATH_OPS_INT(Int2VarPayload, int2Value);
};
struct Int3VarPayload : public CBVarPayload {
  CB_PAYLOAD_CTORS1(Int3VarPayload, int32_t, int3Value, 3);
  CB_PAYLOAD_MATH_OPS(Int3VarPayload, int3Value);
  CB_PAYLOAD_MATH_OPS_INT(Int3VarPayload, int3Value);
};
struct Int4VarPayload : public CBVarPayload {
  CB_PAYLOAD_CTORS1(Int4VarPayload, int32_t, int4Value, 4);
  CB_PAYLOAD_MATH_OPS(Int4VarPayload, int4Value);
  CB_PAYLOAD_MATH_OPS_INT(Int4VarPayload, int4Value);
};
struct Int8VarPayload : public CBVarPayload {
  CB_PAYLOAD_CTORS1(Int8VarPayload, int16_t, int8Value, 8);
  CB_PAYLOAD_MATH_OPS(Int8VarPayload, int8Value);
  CB_PAYLOAD_MATH_OPS_INT(Int8VarPayload, int8Value);
};
struct Int16VarPayload : public CBVarPayload {
  CB_PAYLOAD_CTORS1(Int16VarPayload, int8_t, int16Value, 16);
  CB_PAYLOAD_MATH_OPS(Int16VarPayload, int16Value);
  CB_PAYLOAD_MATH_OPS_INT(Int16VarPayload, int16Value);
};
struct FloatVarPayload : public CBVarPayload {
  CB_PAYLOAD_CTORS0(FloatVarPayload, double, floatValue);
  CB_PAYLOAD_MATH_OPS(FloatVarPayload, floatValue);
  CB_PAYLOAD_MATH_OPS_SIMPLE(FloatVarPayload, floatValue);
};
struct Float2VarPayload : public CBVarPayload {
  CB_PAYLOAD_CTORS1(Float2VarPayload, double, float2Value, 2);
  CB_PAYLOAD_MATH_OPS(Float2VarPayload, float2Value);
  CB_PAYLOAD_MATH_OPS_FLOAT(Float2VarPayload, float2Value);
  CB_PAYLOAD_MATH_POW_FLOAT(Float2VarPayload, float2Value);
};
struct Float3VarPayload : public CBVarPayload {
  CB_PAYLOAD_CTORS1(Float3VarPayload, float, float3Value, 3);
  CB_PAYLOAD_MATH_OPS(Float3VarPayload, float3Value);
  CB_PAYLOAD_MATH_OPS_FLOAT(Float3VarPayload, float3Value);
  CB_PAYLOAD_MATH_POW_FLOAT(Float3VarPayload, float3Value);
};
struct Float4VarPayload : public CBVarPayload {
  CB_PAYLOAD_CTORS1(Float4VarPayload, float, float4Value, 4);
  CB_PAYLOAD_MATH_OPS(Float4VarPayload, float4Value);
  CB_PAYLOAD_MATH_OPS_FLOAT(Float4VarPayload, float4Value);
  CB_PAYLOAD_MATH_POW_FLOAT(Float4VarPayload, float4Value);
};

// forward declare this as we use it in Var
class Chain;

struct Var : public CBVar {
  Var(const IntVarPayload &p) : CBVar() {
    this->valueType = CBType::Int;
    this->payload.intValue = p.intValue;
  }

  Var &operator=(const IntVarPayload &p) {
    this->valueType = CBType::Int;
    this->payload.intValue = p.intValue;
    return *this;
  }

  Var(const Int2VarPayload &p) : CBVar() {
    this->valueType = CBType::Int2;
    memcpy(&this->payload, &p, sizeof(CBVarPayload));
  }

  Var &operator=(const Int2VarPayload &p) {
    this->valueType = CBType::Int2;
    memcpy(&this->payload, &p, sizeof(CBVarPayload));
    return *this;
  }

  Var(const Int3VarPayload &p) : CBVar() {
    this->valueType = CBType::Int3;
    memcpy(&this->payload, &p, sizeof(CBVarPayload));
  }

  Var &operator=(const Int3VarPayload &p) {
    this->valueType = CBType::Int3;
    memcpy(&this->payload, &p, sizeof(CBVarPayload));
    return *this;
  }

  Var(const Int4VarPayload &p) : CBVar() {
    this->valueType = CBType::Int4;
    memcpy(&this->payload, &p, sizeof(CBVarPayload));
  }

  Var &operator=(const Int4VarPayload &p) {
    this->valueType = CBType::Int4;
    memcpy(&this->payload, &p, sizeof(CBVarPayload));
    return *this;
  }

  Var(const Int8VarPayload &p) : CBVar() {
    this->valueType = CBType::Int8;
    memcpy(&this->payload, &p, sizeof(CBVarPayload));
  }

  Var &operator=(const Int8VarPayload &p) {
    this->valueType = CBType::Int8;
    memcpy(&this->payload, &p, sizeof(CBVarPayload));
    return *this;
  }

  Var(const Int16VarPayload &p) : CBVar() {
    this->valueType = CBType::Int16;
    memcpy(&this->payload, &p, sizeof(CBVarPayload));
  }

  Var &operator=(const Int16VarPayload &p) {
    this->valueType = CBType::Int16;
    memcpy(&this->payload, &p, sizeof(CBVarPayload));
    return *this;
  }

  Var(const FloatVarPayload &p) : CBVar() {
    this->valueType = CBType::Float;
    this->payload.floatValue = p.floatValue;
  }

  Var &operator=(const FloatVarPayload &p) {
    this->valueType = CBType::Float;
    this->payload.floatValue = p.floatValue;
    return *this;
  }

  Var(const Float2VarPayload &p) : CBVar() {
    this->valueType = CBType::Float2;
    memcpy(&this->payload, &p, sizeof(CBVarPayload));
  }

  Var &operator=(const Float2VarPayload &p) {
    this->valueType = CBType::Float2;
    memcpy(&this->payload, &p, sizeof(CBVarPayload));
    return *this;
  }

  Var(const Float3VarPayload &p) : CBVar() {
    this->valueType = CBType::Float3;
    memcpy(&this->payload, &p, sizeof(CBVarPayload));
  }

  Var &operator=(const Float3VarPayload &p) {
    this->valueType = CBType::Float3;
    memcpy(&this->payload, &p, sizeof(CBVarPayload));
    return *this;
  }

  Var(const Float4VarPayload &p) : CBVar() {
    this->valueType = CBType::Float4;
    memcpy(&this->payload, &p, sizeof(CBVarPayload));
  }

  Var &operator=(const Float4VarPayload &p) {
    this->valueType = CBType::Float4;
    memcpy(&this->payload, &p, sizeof(CBVarPayload));
    return *this;
  }

  constexpr Var() : CBVar() {}

  explicit Var(const CBVar &other) {
    memcpy((void *)this, (void *)&other, sizeof(CBVar));
  }

  explicit operator bool() const {
    if (valueType != Bool) {
      throw InvalidVarTypeError("Invalid variable casting! expected Bool");
    }
    return payload.boolValue;
  }

  explicit operator int() const {
    if (valueType != Int) {
      throw InvalidVarTypeError("Invalid variable casting! expected Int");
    }
    return static_cast<int>(payload.intValue);
  }

  explicit operator uintptr_t() const {
    if (valueType != Int) {
      throw InvalidVarTypeError("Invalid variable casting! expected Int");
    }
    return static_cast<uintptr_t>(payload.intValue);
  }

  explicit operator int16_t() const {
    if (valueType != Int) {
      throw InvalidVarTypeError("Invalid variable casting! expected Int");
    }
    return static_cast<int16_t>(payload.intValue);
  }

  explicit operator uint8_t() const {
    if (valueType != Int) {
      throw InvalidVarTypeError("Invalid variable casting! expected Int");
    }
    return static_cast<uint8_t>(payload.intValue);
  }

  explicit operator int64_t() const {
    if (valueType != Int) {
      throw InvalidVarTypeError("Invalid variable casting! expected Int");
    }
    return payload.intValue;
  }

  explicit operator float() const {
    if (valueType == Float) {
      return static_cast<float>(payload.floatValue);
    } else if (valueType == Int) {
      return static_cast<float>(payload.intValue);
    } else {
      throw InvalidVarTypeError("Invalid variable casting! expected Float");
    }
  }

  explicit operator double() const {
    if (valueType == Float) {
      return payload.floatValue;
    } else if (valueType == Int) {
      return static_cast<double>(payload.intValue);
    } else {
      throw InvalidVarTypeError("Invalid variable casting! expected Float");
    }
  }

  template <typename T> explicit operator std::vector<T>() const {
    if (valueType != Seq) {
      throw InvalidVarTypeError("Invalid variable casting! expected Seq");
    }
    std::vector<T> res;
    res.resize(payload.seqValue.len);
    for (uint32_t i = 0; i < payload.seqValue.len; i++) {
      res[i] = T(*reinterpret_cast<Var *>(&payload.seqValue.elements[i]));
    }
    return res;
  }

  explicit operator std::vector<Var>() const {
    if (valueType != Seq) {
      throw InvalidVarTypeError("Invalid variable casting! expected Seq");
    }
    std::vector<Var> res{size_t(payload.seqValue.len)};
    for (uint32_t i = 0; i < payload.seqValue.len; i++) {
      res[i] = *reinterpret_cast<Var *>(&payload.seqValue.elements[i]);
    }
    return res;
  }

  constexpr static CBVar Empty{};
  constexpr static CBVar True{{true}, nullptr, 0, CBType::Bool};
  constexpr static CBVar False{{false}, nullptr, 0, CBType::Bool};
  // this is a special one used to flag default value in Chain/Blocks C++ DSL
  constexpr static CBVar Any{{false}, nullptr, 0, CBType::Any};

  template <typename T>
  static Var Object(T valuePtr, uint32_t objectVendorId,
                    uint32_t objectTypeId) {
    Var res;
    res.valueType = CBType::Object;
    res.payload.objectValue = valuePtr;
    res.payload.objectVendorId = objectVendorId;
    res.payload.objectTypeId = objectTypeId;
    return res;
  }

  template <typename T>
  static Var Enum(T value, uint32_t enumVendorId, uint32_t enumTypeId) {
    Var res;
    res.valueType = CBType::Enum;
    res.payload.enumValue = CBEnum(value);
    res.payload.enumVendorId = enumVendorId;
    res.payload.enumTypeId = enumTypeId;
    return res;
  }

  Var(uint8_t *ptr, uint32_t size) : CBVar() {
    valueType = Bytes;
    payload.bytesSize = size;
    payload.bytesValue = ptr;
  }

  Var(const std::vector<uint8_t> &bytes) : CBVar() {
    valueType = Bytes;
    const auto size = bytes.size();
    if (size > UINT32_MAX)
      throw CBException(
          "std::vector<uint8_t> to Var size exceeded uint32 maximum");
    payload.bytesSize = uint32_t(size);
    payload.bytesValue = const_cast<uint8_t *>(bytes.data());
  }

  Var(uint8_t *data, uint16_t width, uint16_t height, uint8_t channels,
      uint8_t flags = 0)
      : CBVar() {
    valueType = CBType::Image;
    payload.imageValue.width = width;
    payload.imageValue.height = height;
    payload.imageValue.channels = channels;
    payload.imageValue.flags = flags;
    payload.imageValue.data = data;
  }

  explicit Var(int src) : CBVar() {
    valueType = Int;
    payload.intValue = src;
  }

  explicit Var(uint8_t src) : CBVar() {
    valueType = Int;
    payload.intValue = int64_t(src);
  }

  explicit Var(char src) : CBVar() {
    valueType = Int;
    payload.intValue = int64_t(src);
  }

  explicit Var(unsigned int src) : CBVar() {
    valueType = Int;
    payload.intValue = int64_t(src);
  }

  explicit Var(int a, int b) : CBVar() {
    valueType = Int2;
    payload.int2Value[0] = a;
    payload.int2Value[1] = b;
  }

  explicit Var(int a, int b, int c) : CBVar() {
    valueType = Int3;
    payload.int3Value[0] = a;
    payload.int3Value[1] = b;
    payload.int3Value[2] = c;
  }

  explicit Var(int a, int b, int c, int d) : CBVar() {
    valueType = Int4;
    payload.int4Value[0] = a;
    payload.int4Value[1] = b;
    payload.int4Value[2] = c;
    payload.int4Value[3] = d;
  }

  explicit Var(int16_t a, int16_t b, int16_t c, int16_t d, int16_t e, int16_t f,
               int16_t g, int16_t h)
      : CBVar() {
    valueType = Int8;
    payload.int8Value[0] = a;
    payload.int8Value[1] = b;
    payload.int8Value[2] = c;
    payload.int8Value[3] = d;
    payload.int8Value[4] = e;
    payload.int8Value[5] = f;
    payload.int8Value[6] = g;
    payload.int8Value[7] = h;
  }

  explicit Var(int8_t a, int8_t b, int8_t c, int8_t d, int8_t e, int8_t f,
               int8_t g, int8_t h, int8_t i, int8_t j, int8_t k, int8_t l,
               int8_t m, int8_t n, int8_t o, int8_t p)
      : CBVar() {
    valueType = Int16;
    payload.int16Value[0] = a;
    payload.int16Value[1] = b;
    payload.int16Value[2] = c;
    payload.int16Value[3] = d;
    payload.int16Value[4] = e;
    payload.int16Value[5] = f;
    payload.int16Value[6] = g;
    payload.int16Value[7] = h;
    payload.int16Value[8] = i;
    payload.int16Value[9] = j;
    payload.int16Value[10] = k;
    payload.int16Value[11] = l;
    payload.int16Value[12] = m;
    payload.int16Value[13] = n;
    payload.int16Value[14] = o;
    payload.int16Value[15] = p;
  }

  explicit Var(int64_t a, int64_t b) : CBVar() {
    valueType = Int2;
    payload.int2Value[0] = a;
    payload.int2Value[1] = b;
  }

  explicit Var(double a, double b) : CBVar() {
    valueType = Float2;
    payload.float2Value[0] = a;
    payload.float2Value[1] = b;
  }

  explicit Var(double a, double b, double c) : CBVar() {
    valueType = Float3;
    payload.float3Value[0] = a;
    payload.float3Value[1] = b;
    payload.float3Value[2] = c;
  }

  explicit Var(double a, double b, double c, double d) : CBVar() {
    valueType = Float4;
    payload.float4Value[0] = a;
    payload.float4Value[1] = b;
    payload.float4Value[2] = c;
    payload.float4Value[3] = d;
  }

  explicit Var(float a, float b) : CBVar() {
    valueType = Float2;
    payload.float2Value[0] = a;
    payload.float2Value[1] = b;
  }

  explicit Var(double src) : CBVar() {
    valueType = Float;
    payload.floatValue = src;
  }

  explicit Var(bool src) : CBVar() {
    valueType = Bool;
    payload.boolValue = src;
  }

  explicit Var(CBSeq seq) : CBVar() {
    valueType = Seq;
    payload.seqValue = seq;
  }

  explicit Var(CBAudio audio) : CBVar() {
    valueType = CBType::Audio;
    payload.audioValue = audio;
  }

  explicit Var(CBChainRef chain) : CBVar() {
    valueType = CBType::Chain;
    payload.chainValue = chain;
  }

  explicit Var(const std::shared_ptr<CBChain> &chain) : CBVar() {
    // INTERNAL USE ONLY, ABI LIKELY NOT COMPATIBLE!
    valueType = CBType::Chain;
    payload.chainValue = reinterpret_cast<CBChainRef>(
        &const_cast<std::shared_ptr<CBChain> &>(chain));
  }

  explicit Var(CBlock *block) : CBVar() {
    // mostly internal use only, anyway use with care!
    valueType = CBType::Block;
    payload.blockValue = block;
  }

  explicit Var(CBImage img) : CBVar() {
    valueType = Image;
    payload.imageValue = img;
  }

  explicit Var(uint64_t src) : CBVar() {
    valueType = Int;
    payload.intValue = src;
  }

  explicit Var(int64_t src) : CBVar() {
    valueType = Int;
    payload.intValue = src;
  }

  explicit Var(const char *src, size_t len = 0) : CBVar() {
    valueType = CBType::String;
    payload.stringValue = src;
    payload.stringLen =
        uint32_t(len == 0 && src != nullptr ? strlen(src) : len);
  }

  explicit Var(const std::string &src) : CBVar() {
    valueType = CBType::String;
    payload.stringValue = src.c_str();
    payload.stringLen = uint32_t(src.length());
  }

  explicit Var(const std::string_view &src) : CBVar() {
    valueType = CBType::String;
    payload.stringValue = src.data();
    payload.stringLen = uint32_t(src.size());
  }

  static Var ContextVar(const std::string &src) {
    Var res{};
    res.valueType = CBType::ContextVar;
    res.payload.stringValue = src.c_str();
    res.payload.stringLen = uint32_t(src.length());
    return res;
  }

  explicit Var(CBTable &src) : CBVar() {
    valueType = Table;
    payload.tableValue = src;
  }

  explicit Var(CBColor color) : CBVar() {
    valueType = Color;
    payload.colorValue = color;
  }

  static Var ColorFromInt(uint32_t rgba) {
    Var res{};
    res.valueType = CBType::Color;
    res.payload.colorValue.r = uint8_t((rgba & 0xFF000000) >> 24);
    res.payload.colorValue.g = uint8_t((rgba & 0x00FF0000) >> 16);
    res.payload.colorValue.b = uint8_t((rgba & 0x0000FF00) >> 8);
    res.payload.colorValue.a = uint8_t((rgba & 0x000000FF) >> 0);
    return res;
  }

  uint32_t colorToInt() {
    if (valueType != Color) {
      throw InvalidVarTypeError("Invalid variable casting! expected Color");
    }
    uint32_t res = 0;
    res |= (uint32_t(payload.colorValue.r) << 24);
    res |= (uint32_t(payload.colorValue.g) << 16);
    res |= (uint32_t(payload.colorValue.b) << 8);
    res |= (uint32_t(payload.colorValue.a) << 0);
    return res;
  }

  explicit Var(const CBVar *data, size_t size) : CBVar() {
    valueType = Seq;
    payload.seqValue.len = uint32_t(size);
    payload.seqValue.elements =
        payload.seqValue.len > 0 ? const_cast<CBVar *>(data) : nullptr;
  }

  template <size_t N>
  explicit Var(const std::array<uint8_t, N> &arrRef) : CBVar() {
    valueType = CBType::Bytes;
    payload.bytesValue = N > 0 ? const_cast<uint8_t *>(arrRef.data()) : nullptr;
    payload.bytesSize = N;
  }

  template <typename TVAR>
  explicit Var(const std::vector<TVAR> &vectorRef) : CBVar() {
    valueType = Seq;
    payload.seqValue.len = uint32_t(vectorRef.size());
    payload.seqValue.elements = payload.seqValue.len > 0
                                    ? const_cast<TVAR *>(vectorRef.data())
                                    : nullptr;
  }

  template <typename TVAR, size_t N>
  explicit Var(const std::array<TVAR, N> &arrRef) : CBVar() {
    valueType = Seq;
    payload.seqValue.elements =
        N > 0 ? const_cast<TVAR *>(arrRef.data()) : nullptr;
    payload.seqValue.len = N;
  }

  Var(const Chain &chain);
};

using VarPayload =
    std::variant<IntVarPayload, Int2VarPayload, Int3VarPayload, Int4VarPayload,
                 Int8VarPayload, Int16VarPayload, FloatVarPayload,
                 Float2VarPayload, Float3VarPayload, Float4VarPayload>;

template <class Function>
inline void ForEach(const CBTable &table, Function &&f) {
  CBTableIterator tit;
  table.api->tableGetIterator(table, &tit);
  CBString k;
  CBVar v;
  while (table.api->tableNext(table, &tit, &k, &v)) {
    f(k, v);
  }
}

template <class Function> inline void ForEach(const CBSet &set, Function &&f) {
  CBSetIterator sit;
  set.api->setGetIterator(set, &sit);
  CBVar v;
  while (set.api->setNext(set, &sit, &v)) {
    f(v);
  }
}

class ChainProvider {
  // used specially for live editing chains, from host languages
public:
  static inline Type NoneType{{CBType::None}};
  static inline Type ProviderType{
      {CBType::Object, {.object = {.vendorId = CoreCC, .typeId = 'chnp'}}}};
  static inline Types ProviderOrNone{{ProviderType, NoneType}};

  ChainProvider() {
    _provider.userData = this;
    _provider.reset = [](CBChainProvider *provider) {
      auto p = reinterpret_cast<ChainProvider *>(provider->userData);
      p->reset();
    };
    _provider.ready = [](CBChainProvider *provider) {
      auto p = reinterpret_cast<ChainProvider *>(provider->userData);
      return p->ready();
    };
    _provider.setup = [](CBChainProvider *provider, const char *path,
                         CBInstanceData data) {
      auto p = reinterpret_cast<ChainProvider *>(provider->userData);
      p->setup(path, data);
    };
    _provider.updated = [](CBChainProvider *provider) {
      auto p = reinterpret_cast<ChainProvider *>(provider->userData);
      return p->updated();
    };
    _provider.acquire = [](CBChainProvider *provider) {
      auto p = reinterpret_cast<ChainProvider *>(provider->userData);
      return p->acquire();
    };
    _provider.release = [](CBChainProvider *provider, CBChain *chain) {
      auto p = reinterpret_cast<ChainProvider *>(provider->userData);
      return p->release(chain);
    };
  }

  virtual ~ChainProvider() {}

  virtual void reset() = 0;

  virtual bool ready() = 0;
  virtual void setup(const char *path, const CBInstanceData &data) = 0;

  virtual bool updated() = 0;
  virtual CBChainProviderUpdate acquire() = 0;

  virtual void release(CBChain *chain) = 0;

  operator CBVar() {
    CBVar res{};
    res.valueType = Object;
    res.payload.objectVendorId = CoreCC;
    res.payload.objectTypeId = 'chnp';
    res.payload.objectValue = &_provider;
    return res;
  }

private:
  CBChainProvider _provider;
};

class Blocks {
public:
  Blocks() {}

  Blocks &block(std::string_view name, std::vector<Var> params);

  template <typename... Vars>
  Blocks &block(std::string_view name, Vars... params) {
    std::vector<Var> vars = {Var(params)...};
    return block(name, vars);
  }

  Blocks &let(Var value);

  template <typename V> Blocks &let(V value) {
    auto val = Var(value);
    return let(val);
  }

  template <typename... Vs> Blocks &let(Vs... values) {
    auto val = Var(values...);
    return let(val);
  }

  operator Var() { return Var(_blocks); }

private:
  std::vector<Var> _blocks;
};

class Chain {
public:
  Chain() {}
  Chain(std::string_view name);

  Chain &block(std::string_view name, std::vector<Var> params);

  template <typename... Vars>
  Chain &block(std::string_view name, Vars... params) {
    std::vector<Var> vars = {Var(params)...};
    return block(name, vars);
  }

  Chain &let(Var value);

  template <typename V> Chain &let(V value) {
    auto val = Var(value);
    return let(val);
  }

  template <typename... Vs> Chain &let(Vs... values) {
    auto val = Var(values...);
    return let(val);
  }

  Chain &looped(bool looped);
  Chain &unsafe(bool unsafe);
  Chain &stackSize(size_t size);
  Chain &name(std::string_view name);

  CBChain *operator->() { return _chain.get(); }
  CBChain *get() { return _chain.get(); }

  operator std::shared_ptr<CBChain>() { return _chain; }
  CBChainRef weakRef() const;

private:
  std::shared_ptr<CBChain> _chain;
};
} // namespace chainblocks

inline CBVar *begin(CBVar &a) {
  if (a.valueType != CBType::Seq) {
    throw chainblocks::CBException("begin expected a Seq");
  }
  return &a.payload.seqValue.elements[0];
}

inline const CBVar *begin(const CBVar &a) {
  if (a.valueType != CBType::Seq) {
    throw chainblocks::CBException("begin expected a Seq");
  }
  return &a.payload.seqValue.elements[0];
}

inline CBVar *end(CBVar &a) {
  if (a.valueType != CBType::Seq) {
    throw chainblocks::CBException("begin expected a Seq");
  }
  return begin(a) + a.payload.seqValue.len;
}

inline const CBVar *end(const CBVar &a) {
  if (a.valueType != CBType::Seq) {
    throw chainblocks::CBException("begin expected a Seq");
  }
  return begin(a) + a.payload.seqValue.len;
}

inline CBExposedTypeInfo *begin(CBExposedTypesInfo &a) {
  return &a.elements[0];
}

inline const CBExposedTypeInfo *begin(const CBExposedTypesInfo &a) {
  return &a.elements[0];
}

inline CBExposedTypeInfo *end(CBExposedTypesInfo &a) {
  return begin(a) + a.len;
}

inline const CBExposedTypeInfo *end(const CBExposedTypesInfo &a) {
  return begin(a) + a.len;
}

inline CBTypeInfo *begin(CBTypesInfo &a) { return &a.elements[0]; }

inline const CBTypeInfo *begin(const CBTypesInfo &a) { return &a.elements[0]; }

inline CBTypeInfo *end(CBTypesInfo &a) { return begin(a) + a.len; }

inline const CBTypeInfo *end(const CBTypesInfo &a) { return begin(a) + a.len; }

#endif
