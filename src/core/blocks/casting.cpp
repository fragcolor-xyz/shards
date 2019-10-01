#include "shared.hpp"

namespace chainblocks {
// TODO Write proper inputTypes Info
#define TO_SOMETHING(_varName_, _width_, _type_, _payload_, _strOp_, _info_)   \
  struct To##_varName_##_width_ {                                              \
    static inline TypesInfo &singleOutput = SharedTypes::_info_;               \
    CBTypesInfo inputTypes() { return CBTypesInfo(SharedTypes::anyInfo); }     \
    CBTypesInfo outputTypes() { return CBTypesInfo(singleOutput); }            \
                                                                               \
    CBTypeInfo inferTypes(CBTypeInfo inputType,                                \
                          CBExposedTypesInfo consumableVariables) {            \
      return CBTypeInfo(singleOutput);                                         \
    }                                                                          \
                                                                               \
    bool convert(CBVar &dst, int &index, const CBVar &src) {                   \
      switch (src.valueType) {                                                 \
      case String: {                                                           \
        dst.payload._payload_[index] =                                         \
            static_cast<_type_>(std::_strOp_(src.payload.stringValue));        \
        index++;                                                               \
        if (index == (_width_))                                                \
          return true;                                                         \
        break;                                                                 \
      }                                                                        \
      case Float:                                                              \
        dst.payload._payload_[index] =                                         \
            static_cast<_type_>(src.payload.floatValue);                       \
        index++;                                                               \
        if (index == (_width_))                                                \
          return true;                                                         \
        break;                                                                 \
      case Float2:                                                             \
        dst.payload._payload_[index] =                                         \
            static_cast<_type_>(src.payload.float2Value[0]);                   \
        index++;                                                               \
        if (index == (_width_))                                                \
          return true;                                                         \
        dst.payload._payload_[index] =                                         \
            static_cast<_type_>(src.payload.float2Value[1]);                   \
        index++;                                                               \
        if (index == (_width_))                                                \
          return true;                                                         \
        break;                                                                 \
      case Float3:                                                             \
        dst.payload._payload_[index] =                                         \
            static_cast<_type_>(src.payload.float3Value[0]);                   \
        index++;                                                               \
        if (index == (_width_))                                                \
          return true;                                                         \
        dst.payload._payload_[index] =                                         \
            static_cast<_type_>(src.payload.float3Value[1]);                   \
        index++;                                                               \
        if (index == (_width_))                                                \
          return true;                                                         \
        dst.payload._payload_[index] =                                         \
            static_cast<_type_>(src.payload.float3Value[2]);                   \
        index++;                                                               \
        if (index == (_width_))                                                \
          return true;                                                         \
        break;                                                                 \
      case Float4:                                                             \
        dst.payload._payload_[index] =                                         \
            static_cast<_type_>(src.payload.float4Value[0]);                   \
        index++;                                                               \
        if (index == (_width_))                                                \
          return true;                                                         \
        dst.payload._payload_[index] =                                         \
            static_cast<_type_>(src.payload.float4Value[1]);                   \
        index++;                                                               \
        if (index == (_width_))                                                \
          return true;                                                         \
        dst.payload._payload_[index] =                                         \
            static_cast<_type_>(src.payload.float4Value[2]);                   \
        index++;                                                               \
        if (index == (_width_))                                                \
          return true;                                                         \
        dst.payload._payload_[index] =                                         \
            static_cast<_type_>(src.payload.float4Value[3]);                   \
        index++;                                                               \
        if (index == (_width_))                                                \
          return true;                                                         \
        break;                                                                 \
      case Int:                                                                \
        dst.payload._payload_[index] =                                         \
            static_cast<_type_>(src.payload.intValue);                         \
        index++;                                                               \
        if (index == (_width_))                                                \
          return true;                                                         \
        break;                                                                 \
      case Int2:                                                               \
        dst.payload._payload_[index] =                                         \
            static_cast<_type_>(src.payload.int2Value[0]);                     \
        index++;                                                               \
        if (index == (_width_))                                                \
          return true;                                                         \
        dst.payload._payload_[index] =                                         \
            static_cast<_type_>(src.payload.int2Value[1]);                     \
        index++;                                                               \
        if (index == (_width_))                                                \
          return true;                                                         \
        break;                                                                 \
      case Int3:                                                               \
        dst.payload._payload_[index] =                                         \
            static_cast<_type_>(src.payload.int3Value[0]);                     \
        index++;                                                               \
        if (index == (_width_))                                                \
          return true;                                                         \
        dst.payload._payload_[index] =                                         \
            static_cast<_type_>(src.payload.int3Value[1]);                     \
        index++;                                                               \
        if (index == (_width_))                                                \
          return true;                                                         \
        dst.payload._payload_[index] =                                         \
            static_cast<_type_>(src.payload.int3Value[2]);                     \
        index++;                                                               \
        if (index == (_width_))                                                \
          return true;                                                         \
        break;                                                                 \
      case Int4:                                                               \
        dst.payload._payload_[index] =                                         \
            static_cast<_type_>(src.payload.int4Value[0]);                     \
        index++;                                                               \
        if (index == (_width_))                                                \
          return true;                                                         \
        dst.payload._payload_[index] =                                         \
            static_cast<_type_>(src.payload.int4Value[1]);                     \
        index++;                                                               \
        if (index == 4)                                                        \
          return true;                                                         \
        dst.payload._payload_[index] =                                         \
            static_cast<_type_>(src.payload.int4Value[2]);                     \
        index++;                                                               \
        if (index == (_width_))                                                \
          return true;                                                         \
        dst.payload._payload_[index] =                                         \
            static_cast<_type_>(src.payload.int4Value[3]);                     \
        index++;                                                               \
        if (index == (_width_))                                                \
          return true;                                                         \
        break;                                                                 \
      default:                                                                 \
        throw CBException("Cannot cast given input type.");                    \
      }                                                                        \
      return false;                                                            \
    }                                                                          \
                                                                               \
    CBVar activate(CBContext *context, const CBVar &input) {                   \
      int index = 0;                                                           \
      CBVar output{};                                                          \
      output.valueType = _varName_##_width_;                                   \
      switch (input.valueType) {                                               \
      case Seq: {                                                              \
        for (auto i = 0;                                                       \
             i < _width_, i < stbds_arrlen(input.payload.seqValue); i++) {     \
          if (convert(output, index, input.payload.seqValue[i]))               \
            return output;                                                     \
        }                                                                      \
        break;                                                                 \
      }                                                                        \
      case Int:                                                                \
      case Int2:                                                               \
      case Int3:                                                               \
      case Int4:                                                               \
      case Float:                                                              \
      case Float2:                                                             \
      case Float3:                                                             \
      case Float4:                                                             \
      case String:                                                             \
        if (convert(output, index, input))                                     \
          return output;                                                       \
        break;                                                                 \
      default:                                                                 \
        throw CBException("Cannot cast given input type.");                    \
      }                                                                        \
      return output;                                                           \
    }                                                                          \
  };                                                                           \
  RUNTIME_CORE_BLOCK(To##_varName_##_width_);                                  \
  RUNTIME_BLOCK_inputTypes(To##_varName_##_width_);                            \
  RUNTIME_BLOCK_outputTypes(To##_varName_##_width_);                           \
  RUNTIME_BLOCK_inferTypes(To##_varName_##_width_);                            \
  RUNTIME_BLOCK_activate(To##_varName_##_width_);                              \
  RUNTIME_BLOCK_END(To##_varName_##_width_);

// TODO improve this
#define TO_SOMETHING_SIMPLE(_varName_, _type_, _payload_, _strOp_, _info_)     \
  struct To##_varName_ {                                                       \
    static inline TypesInfo &singleOutput = SharedTypes::_info_;               \
    CBTypesInfo inputTypes() { return CBTypesInfo(SharedTypes::anyInfo); }     \
    CBTypesInfo outputTypes() { return CBTypesInfo(singleOutput); }            \
                                                                               \
    CBTypeInfo inferTypes(CBTypeInfo inputType,                                \
                          CBExposedTypesInfo consumableVariables) {            \
      return CBTypeInfo(singleOutput);                                         \
    }                                                                          \
                                                                               \
    bool convert(CBVar &dst, const CBVar &src) {                               \
      switch (src.valueType) {                                                 \
      case String: {                                                           \
        dst.payload._payload_ =                                                \
            static_cast<_type_>(std::_strOp_(src.payload.stringValue));        \
        break;                                                                 \
      }                                                                        \
      case Float:                                                              \
        dst.payload._payload_ = static_cast<_type_>(src.payload.floatValue);   \
        break;                                                                 \
      case Float2:                                                             \
        dst.payload._payload_ =                                                \
            static_cast<_type_>(src.payload.float2Value[0]);                   \
        break;                                                                 \
      case Float3:                                                             \
        dst.payload._payload_ =                                                \
            static_cast<_type_>(src.payload.float3Value[0]);                   \
        break;                                                                 \
      case Float4:                                                             \
        dst.payload._payload_ =                                                \
            static_cast<_type_>(src.payload.float4Value[0]);                   \
        break;                                                                 \
      case Int:                                                                \
        dst.payload._payload_ = static_cast<_type_>(src.payload.intValue);     \
        break;                                                                 \
      case Int2:                                                               \
        dst.payload._payload_ = static_cast<_type_>(src.payload.int2Value[0]); \
        break;                                                                 \
      case Int3:                                                               \
        dst.payload._payload_ = static_cast<_type_>(src.payload.int3Value[0]); \
        break;                                                                 \
      case Int4:                                                               \
        dst.payload._payload_ = static_cast<_type_>(src.payload.int4Value[0]); \
        break;                                                                 \
      default:                                                                 \
        throw CBException("Cannot cast given input type.");                    \
      }                                                                        \
      return false;                                                            \
    }                                                                          \
                                                                               \
    CBVar activate(CBContext *context, const CBVar &input) {                   \
      CBVar output{};                                                          \
      output.valueType = _varName_;                                            \
      switch (input.valueType) {                                               \
      case Seq: {                                                              \
        for (auto i = 0; i < 1, i < stbds_arrlen(input.payload.seqValue);      \
             i++) {                                                            \
          if (convert(output, input.payload.seqValue[i]))                      \
            return output;                                                     \
        }                                                                      \
        break;                                                                 \
      }                                                                        \
      case Int:                                                                \
      case Int2:                                                               \
      case Int3:                                                               \
      case Int4:                                                               \
      case Float:                                                              \
      case Float2:                                                             \
      case Float3:                                                             \
      case Float4:                                                             \
      case String:                                                             \
        if (convert(output, input))                                            \
          return output;                                                       \
        break;                                                                 \
      default:                                                                 \
        throw CBException("Cannot cast given input type.");                    \
      }                                                                        \
      return output;                                                           \
    }                                                                          \
  };                                                                           \
  RUNTIME_CORE_BLOCK(To##_varName_);                                           \
  RUNTIME_BLOCK_inputTypes(To##_varName_);                                     \
  RUNTIME_BLOCK_outputTypes(To##_varName_);                                    \
  RUNTIME_BLOCK_inferTypes(To##_varName_);                                     \
  RUNTIME_BLOCK_activate(To##_varName_);                                       \
  RUNTIME_BLOCK_END(To##_varName_);

TO_SOMETHING_SIMPLE(Int, int64_t, intValue, stoi, intInfo);
TO_SOMETHING(Int, 2, int64_t, int2Value, stoi, int2Info);
TO_SOMETHING(Int, 3, int32_t, int3Value, stoi, int3Info);
TO_SOMETHING(Int, 4, int32_t, int4Value, stoi, int4Info);
TO_SOMETHING_SIMPLE(Float, double, floatValue, stod, floatInfo);
TO_SOMETHING(Float, 2, double, float2Value, stod, float2Info);
TO_SOMETHING(Float, 3, float, float3Value, stof, float3Info);
TO_SOMETHING(Float, 4, float, float4Value, stof, float4Info);

struct ToString {
  VarStringStream stream;
  static CBTypesInfo inputTypes() { return CBTypesInfo(CoreInfo::anyInfo); }
  static CBTypesInfo outputTypes() { return CBTypesInfo(CoreInfo::strInfo); }
  CBVar activate(CBContext *context, const CBVar &input) {
    stream.write(input);
    return Var(stream.str());
  }
};

struct ToHex {
  VarStringStream stream;
  static CBTypesInfo inputTypes() { return CBTypesInfo(CoreInfo::intInfo); }
  static CBTypesInfo outputTypes() { return CBTypesInfo(CoreInfo::strInfo); }
  CBVar activate(CBContext *context, const CBVar &input) {
    stream.tryWriteHex(input);
    return Var(stream.str());
  }
};

struct VarAddr {
  VarStringStream stream;
  static CBTypesInfo inputTypes() { return CBTypesInfo(CoreInfo::strInfo); }
  static CBTypesInfo outputTypes() { return CBTypesInfo(CoreInfo::intInfo); }
  CBVar activate(CBContext *context, const CBVar &input) {
    auto v = contextVariable(context, input.payload.stringValue);
    return Var(reinterpret_cast<int64_t>(v));
  }
};

// As, reinterpret
#define AS_SOMETHING_SIMPLE(_varName_, _varName_2, _type_, _payload_, _strOp_, \
                            _info_)                                            \
  struct As##_varName_2 {                                                      \
    static inline TypesInfo &singleOutput = SharedTypes::_info_;               \
    CBTypesInfo inputTypes() { return CBTypesInfo(SharedTypes::anyInfo); }     \
    CBTypesInfo outputTypes() { return CBTypesInfo(singleOutput); }            \
                                                                               \
    CBTypeInfo inferTypes(CBTypeInfo inputType,                                \
                          CBExposedTypesInfo consumableVariables) {            \
      return CBTypeInfo(singleOutput);                                         \
    }                                                                          \
                                                                               \
    bool convert(CBVar &dst, CBVar &src) {                                     \
      switch (src.valueType) {                                                 \
      case String: {                                                           \
        auto val = std::_strOp_(src.payload.stringValue);                      \
        dst.payload._payload_ = reinterpret_cast<_type_ &>(val);               \
        break;                                                                 \
      }                                                                        \
      case Float:                                                              \
        dst.payload._payload_ =                                                \
            reinterpret_cast<_type_ &>(src.payload.floatValue);                \
        break;                                                                 \
      case Float2: {                                                           \
        auto val = src.payload.float2Value[0];                                 \
        dst.payload._payload_ = reinterpret_cast<_type_ &>(val);               \
        break;                                                                 \
      }                                                                        \
      case Float3: {                                                           \
        auto val = src.payload.float3Value[0];                                 \
        dst.payload._payload_ = reinterpret_cast<_type_ &>(val);               \
        break;                                                                 \
      }                                                                        \
      case Float4: {                                                           \
        auto val = src.payload.float4Value[0];                                 \
        dst.payload._payload_ = reinterpret_cast<_type_ &>(val);               \
        break;                                                                 \
      }                                                                        \
      case Int:                                                                \
        dst.payload._payload_ =                                                \
            reinterpret_cast<_type_ &>(src.payload.intValue);                  \
        break;                                                                 \
      case Int2: {                                                             \
        auto val = src.payload.int2Value[0];                                   \
        dst.payload._payload_ = reinterpret_cast<_type_ &>(val);               \
        break;                                                                 \
      }                                                                        \
      case Int3: {                                                             \
        auto val = src.payload.int3Value[0];                                   \
        dst.payload._payload_ = reinterpret_cast<_type_ &>(val);               \
        break;                                                                 \
      }                                                                        \
      case Int4: {                                                             \
        auto val = src.payload.int4Value[0];                                   \
        dst.payload._payload_ = reinterpret_cast<_type_ &>(val);               \
        break;                                                                 \
      }                                                                        \
      default:                                                                 \
        throw CBException("Cannot cast given input type.");                    \
      }                                                                        \
      return false;                                                            \
    }                                                                          \
                                                                               \
    CBVar activate(CBContext *context, const CBVar &input) {                   \
      CBVar output{};                                                          \
      output.valueType = _varName_;                                            \
      switch (input.valueType) {                                               \
      case Seq: {                                                              \
        for (auto i = 0; i < 1, i < stbds_arrlen(input.payload.seqValue);      \
             i++) {                                                            \
          if (convert(output, input.payload.seqValue[i]))                      \
            return output;                                                     \
        }                                                                      \
        break;                                                                 \
      }                                                                        \
      case Int:                                                                \
      case Int2:                                                               \
      case Int3:                                                               \
      case Int4:                                                               \
      case Float:                                                              \
      case Float2:                                                             \
      case Float3:                                                             \
      case Float4:                                                             \
      case String:                                                             \
        if (convert(output, const_cast<CBVar &>(input)))                       \
          return output;                                                       \
        break;                                                                 \
      default:                                                                 \
        throw CBException("Cannot cast given input type.");                    \
      }                                                                        \
      return output;                                                           \
    }                                                                          \
  };                                                                           \
  RUNTIME_CORE_BLOCK(As##_varName_2);                                          \
  RUNTIME_BLOCK_inputTypes(As##_varName_2);                                    \
  RUNTIME_BLOCK_outputTypes(As##_varName_2);                                   \
  RUNTIME_BLOCK_inferTypes(As##_varName_2);                                    \
  RUNTIME_BLOCK_activate(As##_varName_2);                                      \
  RUNTIME_BLOCK_END(As##_varName_2);

AS_SOMETHING_SIMPLE(Int, Int32, int32_t, intValue, stoi, intInfo);
AS_SOMETHING_SIMPLE(Int, Int64, int64_t, intValue, stoi, intInfo);
AS_SOMETHING_SIMPLE(Float, Float32, float, floatValue, stof, floatInfo);
AS_SOMETHING_SIMPLE(Float, Float64, double, floatValue, stod, floatInfo);

// Register ToString
RUNTIME_CORE_BLOCK(ToString);
RUNTIME_BLOCK_inputTypes(ToString);
RUNTIME_BLOCK_outputTypes(ToString);
RUNTIME_BLOCK_activate(ToString);
RUNTIME_BLOCK_END(ToString);

// Register ToHex
RUNTIME_CORE_BLOCK(ToHex);
RUNTIME_BLOCK_inputTypes(ToHex);
RUNTIME_BLOCK_outputTypes(ToHex);
RUNTIME_BLOCK_activate(ToHex);
RUNTIME_BLOCK_END(ToHex);

// Register VarAddr
RUNTIME_CORE_BLOCK(VarAddr);
RUNTIME_BLOCK_inputTypes(VarAddr);
RUNTIME_BLOCK_outputTypes(VarAddr);
RUNTIME_BLOCK_activate(VarAddr);
RUNTIME_BLOCK_END(VarAddr);

void registerCastingBlocks() {
  REGISTER_CORE_BLOCK(ToInt);
  REGISTER_CORE_BLOCK(ToInt2);
  REGISTER_CORE_BLOCK(ToInt3);
  REGISTER_CORE_BLOCK(ToInt4);
  REGISTER_CORE_BLOCK(ToFloat);
  REGISTER_CORE_BLOCK(ToFloat2);
  REGISTER_CORE_BLOCK(ToFloat3);
  REGISTER_CORE_BLOCK(ToFloat4);
  REGISTER_CORE_BLOCK(ToString);
  REGISTER_CORE_BLOCK(ToHex);
  REGISTER_CORE_BLOCK(VarAddr);
  REGISTER_CORE_BLOCK(AsInt32);
  REGISTER_CORE_BLOCK(AsInt64);
  REGISTER_CORE_BLOCK(AsFloat32);
  REGISTER_CORE_BLOCK(AsFloat64);
}
}; // namespace chainblocks
