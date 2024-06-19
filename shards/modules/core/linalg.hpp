#pragma once

#include <shards/common_types.hpp>
#include <shards/core/foundation.hpp>
#include <shards/shards.h>
#include <shards/core/shared.hpp>
#include <shards/gfx/linalg.hpp>
#include "math.hpp"
#include "core.hpp"
#include <shards/core/params.hpp>
#include <shards/linalg_shim.hpp>
#include <math.h>
#include <cmath>
#include <stdexcept>

namespace shards {
namespace Math {
namespace LinAlg {

struct Types {
  static inline std::array<SHVar, 3> _trsKeys{
      Var("translation"),
      Var("rotation"),
      Var("scale"),
  };
  static inline shards::Types _trsTypes{{
      CoreInfo::Float3Type,
      CoreInfo::Float4Type,
      CoreInfo::Float3Type,
  }};
  static inline Type TRS = Type::TableOf(_trsTypes, _trsKeys);
};

template <typename TOp> struct UnaryOperation : public Math::UnaryOperation<TOp> {
  static SHTypesInfo inputTypes() { return CoreInfo::FloatVectors; }
  static SHTypesInfo outputTypes() { return CoreInfo::FloatVectors; }
};

template <typename TOp> struct BinaryOperation : public Math::BinaryOperation<TOp> {
  static SHTypesInfo inputTypes() { return CoreInfo::FloatVectors; }
  static SHTypesInfo outputTypes() { return CoreInfo::FloatVectors; }

  static inline ParamsInfo paramsInfo =
      ParamsInfo(ParamsInfo::Param("Operand", SHCCSTR("The operand."), CoreInfo::FloatVectorsOrVar));

  static SHParametersInfo parameters() { return SHParametersInfo(paramsInfo); }
};

SH_HAS_MEMBER_TEST(validateTypes);

template <typename TOp> struct VectorBinaryOperation {
  TOp op{};
  const VectorTypeTraits *_lhsVecType{};
  const VectorTypeTraits *_rhsVecType{};

  OpType validateTypes(const SHTypeInfo &lhs, const SHType &rhs, SHTypeInfo &resultType) {
    OpType opType = OpType::Invalid;

    if constexpr (has_validateTypes<TOp>::value) {
      opType = op.validateTypes(lhs, rhs, resultType);
      if (opType != OpType::Invalid)
        return opType;
    }

    if (rhs != SHType::Seq && lhs.basicType != SHType::Seq) {
      opType = OpType::Direct;
      resultType = lhs;
    }

    return opType;
  }

  void operateDirect(SHVar &output, const SHVar &a, const SHVar &b) { op.apply(output, a, b); }

  void operateBroadcast(SHVar &output, const SHVar &a, const SHVar &b) {
    throw std::logic_error("broadcast not allowed on vector operations");
  }
};

template <typename TOp> struct VectorUnaryOperation {
  TOp op{};
  OpType validateTypes(const SHTypeInfo &a, SHTypeInfo &resultType) {
    OpType opType = OpType::Invalid;

    if constexpr (has_validateTypes<TOp>::value) {
      opType = op.validateTypes(a, resultType);
      if (opType != OpType::Invalid)
        return opType;
    }

    if (a.basicType != SHType::Seq) {
      opType = OpType::Direct;
      resultType = a;
    }

    return opType;
  }

  void operateDirect(SHVar &output, const SHVar &a) { op.apply(output, a); }
};

struct CrossOp {
  void apply(SHVar &output, const SHVar &input, const SHVar &operand);
};
using Cross = BinaryOperation<VectorBinaryOperation<CrossOp>>;

inline OpType validateTypesVectorToFloat(const SHTypeInfo &lhs, const SHType &rhs, SHTypeInfo &resultType) {
  if (lhs.basicType != SHType::Seq && rhs != SHType::Seq) {
    resultType = CoreInfo::FloatType;
    return OpType::Direct;
  }
  return OpType::Invalid;
}

inline OpType validateTypesVectorToFloat(const SHTypeInfo &lhs, SHTypeInfo &resultType) {
  return validateTypesVectorToFloat(lhs, lhs.basicType, resultType);
}

struct DotOp {
  OpType validateTypes(const SHTypeInfo &lhs, const SHType &rhs, SHTypeInfo &resultType) {
    return validateTypesVectorToFloat(lhs, rhs, resultType);
  }

  void apply(SHVar &output, const SHVar &input, const SHVar &operand);
};
struct Dot : public BinaryOperation<VectorBinaryOperation<DotOp>> {
  static SHTypesInfo outputTypes() { return CoreInfo::FloatType; }
};

struct LengthSquaredOp {
  DotOp dotOp;

  OpType validateTypes(const SHTypeInfo &lhs, SHTypeInfo &resultType) { return validateTypesVectorToFloat(lhs, resultType); }

  void apply(SHVar &output, const SHVar &input) { dotOp.apply(output, input, input); }
};
struct LengthSquared : UnaryOperation<VectorUnaryOperation<LengthSquaredOp>> {
  static SHTypesInfo outputTypes() { return CoreInfo::FloatType; }
};

struct LengthOp {
  LengthSquaredOp lenOp;

  OpType validateTypes(const SHTypeInfo &lhs, SHTypeInfo &resultType) { return validateTypesVectorToFloat(lhs, resultType); }

  void apply(SHVar &output, const SHVar &input) {
    lenOp.apply(output, input);
    output.payload.floatValue = __builtin_sqrt(output.payload.floatValue);
  }
};
struct Length : public UnaryOperation<VectorUnaryOperation<LengthOp>> {
  static SHTypesInfo outputTypes() { return CoreInfo::FloatType; }
};

struct NormalizeOp {
  bool positiveOnly{false};
  LengthOp lenOp;
  void apply(SHVar &output, const SHVar &input);
};

struct Normalize : public UnaryOperation<VectorUnaryOperation<NormalizeOp>> {
  // Normalize also supports SHType::Float seqs
  static SHTypesInfo inputTypes() { return CoreInfo::FloatVectorsOrFloatSeq; }
  static SHTypesInfo outputTypes() { return CoreInfo::FloatVectorsOrFloatSeq; }

  static inline Parameters Params{
      {"Positive", SHCCSTR("If the output should be in the range 0.0~1.0 instead of -1.0~1.0."), {CoreInfo::BoolType}}};
  SHParametersInfo parameters() { return Params; }

  void setParam(int index, const SHVar &value) { op.op.positiveOnly = value.payload.boolValue; }
  SHVar getParam(int index) { return Var(op.op.positiveOnly); }

  SHTypeInfo compose(const SHInstanceData &data) {
    if (data.inputType.basicType == SHType::Seq && data.inputType.seqTypes.len == 1 &&
        data.inputType.seqTypes.elements[0].basicType == SHType::Float) {
      OVERRIDE_ACTIVATE(data, activateFloatSeq);
      return data.inputType;
    } else {
      return UnaryOperation::compose(data);
    }
  }

  SHVar activateFloatSeq(SHContext *context, const SHVar &input);
};

// MatMul is special...
// Mat @ Mat = Mat
// Mat @ Vec = Vec
// If ever becomes a bottle neck, valgrind and optimize
struct MatMul : public BinaryBase {
  OpType validateTypes(const SHTypeInfo &lhs, const SHType &rhs, SHTypeInfo &resultType) {
    if (lhs.basicType == SHType::Seq && rhs == SHType::Seq) {
      resultType = lhs;
      return OpType::SeqSeq;
    } else {
      if (lhs.seqTypes.len != 1) {
        throw SHException("MatMul expected a unique Seq inner type.");
      }
      resultType = lhs.seqTypes.elements[0];
      return OpType::Seq1;
    }

    return OpType::Invalid;
  }

  SHTypeInfo compose(const SHInstanceData &data) { return this->genericCompose(*this, data); }

  SHVar activate(SHContext *context, const SHVar &input);
};

struct Transpose : public UnaryBase {
  OpType validateTypes(const SHTypeInfo &lhs, SHTypeInfo &resultType) {
    if (lhs.basicType == SHType::Seq) {
      resultType = lhs;
      return OpType::Seq1;
    }
    return OpType::Invalid;
  }

  SHTypeInfo compose(const SHInstanceData &data) {
    SHTypeInfo resultType = data.inputType;
    _opType = validateTypes(data.inputType, resultType);

    if (_opType == Invalid) {
      throw ComposeError("Incompatible type for Transpose");
    }

    return resultType;
  }

  SHVar activate(SHContext *context, const SHVar &input);
};

struct Inverse : public UnaryBase {
  static inline shards::Types MatrixTypes{{CoreInfo::Float4x4Type}};
  static SHTypesInfo inputTypes() { return MatrixTypes; }
  static SHTypesInfo outputTypes() { return MatrixTypes; }

  OpType validateTypes(const SHTypeInfo &lhs, SHTypeInfo &resultType) {
    if (lhs.basicType == SHType::Seq) {
      resultType = lhs;
      return OpType::Seq1;
    }
    return OpType::Invalid;
  }

  SHTypeInfo compose(const SHInstanceData &data) {
    SHTypeInfo resultType = data.inputType;
    _opType = validateTypes(data.inputType, resultType);

    if (_opType == Invalid) {
      throw ComposeError("Incompatible type for Transpose");
    }

    return resultType;
  }

  SHVar activate(SHContext *context, const SHVar &input);
};

struct Orthographic {
  double _width = 1280;
  double _height = 720;
  double _near = 0.0;
  double _far = 1000.0;

  Mat4 _output{};

  static SHTypesInfo inputTypes() { return CoreInfo::NoneType; }
  static SHTypesInfo outputTypes() { return CoreInfo::Float4x4Type; }

  // left, right, bottom, top, near, far
  static inline ParamsInfo params = ParamsInfo(ParamsInfo::Param("Width", SHCCSTR("Width size."), CoreInfo::IntOrFloat),
                                               ParamsInfo::Param("Height", SHCCSTR("Height size."), CoreInfo::IntOrFloat),
                                               ParamsInfo::Param("Near", SHCCSTR("Near plane."), CoreInfo::IntOrFloat),
                                               ParamsInfo::Param("Far", SHCCSTR("Far plane."), CoreInfo::IntOrFloat));

  static SHParametersInfo parameters() { return SHParametersInfo(params); }

  void setParam(int index, const SHVar &value) {
    switch (index) {
    case 0:
      _width = double(Var(value));
      break;
    case 1:
      _height = double(Var(value));
      break;
    case 2:
      _near = double(Var(value));
      break;
    case 3:
      _far = double(Var(value));
    default:
      break;
    }
  }

  SHVar getParam(int index) {
    switch (index) {
    case 0:
      return Var(_width);
    case 1:
      return Var(_height);
    case 2:
      return Var(_near);
    case 3:
      return Var(_far);
    default:
      break;
    }
    return Var();
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    auto right = 0.5 * _width;
    auto left = -right;
    auto top = 0.5 * _height;
    auto bottom = -top;
    auto zrange = 1.0 / (_far - _near);
    _output[0][0] = 2.0 / (right - left);
    _output[1][1] = 2.0 / (top - bottom);
    _output[2][2] = -zrange;
    _output[3][0] = (left + right) / (left - right);
    _output[3][1] = (top + bottom) / (bottom - top);
    _output[3][2] = -_near * zrange;
    _output[3][3] = 1.0;
    return _output;
  }
};

// TODO take opposite type as input and decompose as well, so that the same
// shard can be used to decompose
struct Translation {
  Mat4 _output{};

  static SHTypesInfo inputTypes() { return CoreInfo::Float3Type; }
  static SHTypesInfo outputTypes() { return CoreInfo::Float4x4Type; }

  SHVar activate(SHContext *context, const SHVar &input) {
    auto v3 = reinterpret_cast<const Vec3 *>(&input);
    _output = linalg::translation_matrix(**v3);
    return _output;
  }
};

struct Scaling {
  Mat4 _output{};

  static SHTypesInfo inputTypes() { return CoreInfo::Float3Type; }
  static SHTypesInfo outputTypes() { return CoreInfo::Float4x4Type; }

  SHVar activate(SHContext *context, const SHVar &input) {
    auto v3 = reinterpret_cast<const Vec3 *>(&input);
    _output = linalg::scaling_matrix(**v3);
    return _output;
  }
};

struct Rotation {
  Mat4 _output{};

  static SHTypesInfo inputTypes() { return CoreInfo::Float4Type; }
  static SHTypesInfo outputTypes() { return CoreInfo::Float4x4Type; }

  SHVar activate(SHContext *context, const SHVar &input) {
    // quaternion
    auto v4 = reinterpret_cast<const Vec4 *>(&input);
    _output = linalg::rotation_matrix(**v4);
    return _output;
  }
};

struct LookAt {
  static inline shards::Types InputTableTypes{{CoreInfo::Float3Type, CoreInfo::Float3Type}};
  static inline std::array<SHVar, 2> InputTableKeys{Var("Position"), Var("Target")};
  static inline Type InputTable = Type::TableOf(InputTableTypes, InputTableKeys);

  static SHTypesInfo inputTypes() { return InputTable; }
  static SHTypesInfo outputTypes() { return CoreInfo::Float4x4Type; }

  SHVar activate(SHContext *context, const SHVar &input) {
    // this is the most efficient way to find items in table
    // without hashing and possible allocations etc
    SHTable table = input.payload.tableValue;
    SHTableIterator it;
    table.api->tableGetIterator(table, &it);
    SHVar position{};
    SHVar target{};
    while (true) {
      SHVar k;
      SHVar v;
      if (!table.api->tableNext(table, &it, &k, &v))
        break;

      if (k.valueType != SHType::String)
        throw ActivationError("LookAt: Table keys must be strings.");

      switch (k.payload.stringValue[0]) {
      case 'P':
        position = v;
        break;
      case 'T':
        target = v;
        break;
      default:
        break;
      }
    }

    assert(position.valueType == SHType::Float3 && target.valueType == SHType::Float3);

    using namespace linalg::aliases;
    auto eye_ = reinterpret_cast<const padded::Float3 *>(&position);
    auto target_ = reinterpret_cast<const padded::Float3 *>(&target);
    _output = gfx::safeLookat(*eye_, *target_);
    return _output;
  }

private:
  Mat4 _output{};
};

template <const linalg::aliases::float3 &AXIS> struct AxisAngle {
  Vec4 _output{};

  static SHTypesInfo inputTypes() { return CoreInfo::FloatType; }
  static SHTypesInfo outputTypes() { return CoreInfo::Float4Type; }

  SHVar activate(SHContext *context, const SHVar &input) {
    _output = linalg::rotation_quat(AXIS, float(input.payload.floatValue));
    return _output;
  }
};

using AxisAngleX = AxisAngle<AxisX>;
using AxisAngleY = AxisAngle<AxisY>;
using AxisAngleZ = AxisAngle<AxisZ>;

struct Deg2Rad {
  const double PI = 3.141592653589793238463;

  static SHTypesInfo inputTypes() { return CoreInfo::FloatType; }
  static SHTypesInfo outputTypes() { return CoreInfo::FloatType; }

  SHVar activate(SHContext *context, const SHVar &input) { return Var(input.payload.floatValue * (PI / 180.0)); }
};

struct Rad2Deg {
  const double PI = 3.141592653589793238463;

  static SHTypesInfo inputTypes() { return CoreInfo::FloatType; }
  static SHTypesInfo outputTypes() { return CoreInfo::FloatType; }

  SHVar activate(SHContext *context, const SHVar &input) { return Var(input.payload.floatValue * (180.0 / PI)); }
};

struct MatIdentity {
  PARAM_VAR(_type, "Type", "The matrix row type of the corresponding matrix", {CoreInfo2::TypeEnumInfo::Type});
  PARAM_IMPL(PARAM_IMPL_FOR(_type));

  static SHOptionalString help() { return SHCCSTR("Gives identity values for square matrix types"); }
  static SHTypesInfo inputTypes() { return CoreInfo::NoneType; }
  static SHTypesInfo outputTypes() { return CoreInfo::Float4x4Type; }

  static inline Mat4 Float4x4Identity = linalg::identity;
  static inline SHVar Float4x4IdentityVar = SHVar(Float4x4Identity);

  MatIdentity() { _type = Var::Enum(BasicTypes::Float4, CoreInfo2::TypeEnumInfo::Type); }

  SHTypeInfo compose(const SHInstanceData &data) {
    switch (BasicTypes(_type.payload.enumValue)) {
    case shards::BasicTypes::Float4:
      return CoreInfo::Float4x4Type;
    default:
      throw std::runtime_error("Unsuported matrix row type");
    }
    return SHTypeInfo{};
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    switch (BasicTypes(_type.payload.enumValue)) {
    case shards::BasicTypes::Float4:
      return Float4x4IdentityVar;
    default:
      return SHVar{};
    }
  }
};

} // namespace LinAlg
} // namespace Math
} // namespace shards
