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

struct Cross : public BinaryOperation<VectorBinaryOperation<CrossOp>> {
  static SHOptionalString help() {
    return SHCCSTR("This shard computes the cross product of the float3 vector provided as input and the float3 vector "
                   "provided in the Operand parameter and returns the result as a float3 vector. A float3 vector is a vector "
                   "with 3 float elements.");
  }

  static SHTypesInfo inputTypes() { return CoreInfo::Float3Type; }
  static SHOptionalString inputHelp() { return SHCCSTR("Accepts float3 vector (a vector with 3 float elements) as input."); }

  static SHTypesInfo outputTypes() { return CoreInfo::Float3Type; }
  static SHOptionalString outputHelp() { return SHCCSTR("Returns the result of the cross product as a float3 vector."); }

  static inline ParamsInfo paramsInfo =
      ParamsInfo(ParamsInfo::Param("Operand", SHCCSTR("The float3 vector to compute the cross product with."), CoreInfo::FloatVectorsOrVar));

  static SHParametersInfo parameters() { return SHParametersInfo(paramsInfo); }
};

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
  static SHOptionalString help() {
    return SHCCSTR(
        "Computes the dot product of two float vectors with an equal number of elements, and returns the resulting float value. "
        "The first float vector is passed as input and the second float vector is specified in the Operand parameter.");
  }

  static SHTypesInfo inputTypes() { return CoreInfo::FloatVectors; }
  static SHOptionalString inputHelp() {
    return SHCCSTR("Takes in a float vector of any dimension (e.g., float2, float3, float4).");
  }

  static SHOptionalString outputHelp() { return SHCCSTR("Returns the resulting dot product as a float value."); }
};

struct LengthSquaredOp {
  DotOp dotOp;

  OpType validateTypes(const SHTypeInfo &lhs, SHTypeInfo &resultType) { return validateTypesVectorToFloat(lhs, resultType); }

  void apply(SHVar &output, const SHVar &input) { dotOp.apply(output, input, input); }
};
struct LengthSquared : UnaryOperation<VectorUnaryOperation<LengthSquaredOp>> {
  static SHTypesInfo inputTypes() { return CoreInfo::FloatVectors; }
  static SHOptionalString inputHelp() {
    return SHCCSTR("Accepts a float vector of any dimension (e.g., float2, float3, float4).");
  }

  static SHTypesInfo outputTypes() { return CoreInfo::FloatType; }
  static SHOptionalString outputHelp() { return SHCCSTR("Returns the squared magnitude of the input vector as a float."); }

  static SHOptionalString help() {
    return SHCCSTR("Computes the squared magnitude of a float vector of any dimension and returns the result as a float.");
  }
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
  static SHTypesInfo inputTypes() { return CoreInfo::FloatVectors; }
  static SHOptionalString inputHelp() {
    return SHCCSTR("Accepts a float vector of any dimension (e.g., float2, float3, float4).");
  }

  static SHTypesInfo outputTypes() { return CoreInfo::FloatType; }
  static SHOptionalString outputHelp() { return SHCCSTR("Returns the magnitude of the input vector as a float."); }

  static SHOptionalString help() {
    return SHCCSTR("Computes the magnitude of a float vector of any dimension and returns the result as a float.");
  }
};

struct NormalizeOp {
  bool positiveOnly{false};
  LengthOp lenOp;
  void apply(SHVar &output, const SHVar &input);
};

struct Normalize : public UnaryOperation<VectorUnaryOperation<NormalizeOp>> {
  // Normalize also supports SHType::Float seqs
  static SHTypesInfo inputTypes() { return CoreInfo::FloatVectorsOrFloatSeq; }
  static SHOptionalString inputHelp() {
    return SHCCSTR("Accepts a float vector of any dimension (e.g., float2, float3, float4) or a float sequence of any length.");
  }

  static SHTypesInfo outputTypes() { return CoreInfo::FloatVectorsOrFloatSeq; }
  static SHOptionalString outputHelp() {
    return SHCCSTR("Returns a float vector of the same dimension or a float sequence of the same length as what was passed as "
                   "input but with its values normalized to a magnitude of 1.");
  }

  static SHOptionalString help() {
    return SHCCSTR("This shard normalizes a float vector of any dimension or a sequence of floats, scaling it to have a "
                   "magnitude of 1 while preserving "
                   "its direction. "
                   "By default, output values can range from -1.0 to 1.0. If the 'Positive' parameter is set to true, the output "
                   "will be scaled to the range 0.0 to 1.0. "
                   "For example, normalizing [4.0 -5.0 6.0 -7.0] will result in [0.3563, -0.4454, 0.5345, -0.6236], which has a "
                   "length of 1. ");
  }

  static inline Parameters Params{{"Positive",
                                   SHCCSTR("If set to true, the output will be in the range 0.0~1.0 instead of -1.0~1.0."),
                                   {CoreInfo::BoolType}}};
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
  static SHOptionalString help() {
    return SHCCSTR("Performs matrix multiplication on either two 4x4 matrices or a 4x4 matrix and a float4 vector and returns "
                   "either a 4x4 matrix or a float4 vector accordingly. A 4x4 matrix is a sequence with exactly 4 float4 vectors "
                   "while a float4 vector is a vector with 4 float elements.");
  }

  static SHTypesInfo inputTypes() { return CoreInfo::Float4x4Type; }
  static SHOptionalString inputHelp() { return SHCCSTR("Takes a 4x4 matrix as input. (a sequence of four float4 vectors)"); }

  static SHTypesInfo outputTypes() { return CoreInfo::MatrixOrVector; }
  static SHOptionalString outputHelp() {
    return SHCCSTR(
        "Returns the result of the matrix multiplication. If a 4x4 matrix is multiplied by a float4 vector, the result is a "
        "float4 vector (a vector with 4 float elements). If two 4x4 matrices are multiplied, the result is a 4x4 matrix (a "
        "sequence of four float4 vectors).");
  }
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
  static SHOptionalString help() {
    return SHCCSTR(
        "Performs matrix transposition on the input matrix. Transposition flips the matrix over its main diagonal, "
        "switching its rows and columns. This shard supports 2x2, 3x3, and 4x4 as input matrices. A 4x4 matrix is a sequence "
        "with exactly 4 float4 vectors, a 3x3 matrix is a sequence with exactly 3 float3 vectors, and a 2x2 matrix "
        "is a sequence with exactly 2 float2 vectors.");
  }

  static SHTypesInfo inputTypes() { return CoreInfo::DifferentMatrixes; }
  static SHOptionalString inputHelp() {
    return SHCCSTR("Takes a matrix (sequence of float2, float3, or float4 vectors) as input.");
  }

  static SHTypesInfo outputTypes() { return CoreInfo::DifferentMatrixes; }
  static SHOptionalString outputHelp() { return SHCCSTR("Returns the transposed the matrix."); }

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
  static SHOptionalString help() {
    return SHCCSTR("This shard takes a 4x4 matrices as input and computes its inverse. A 4x4 matrix is a sequence with exactly 4 "
                   "float4 vectors while a float4 vector is a vector with 4 float elements.");
  }

  static SHTypesInfo inputTypes() { return CoreInfo::Float4x4Type; }
  static SHOptionalString inputHelp() { return SHCCSTR("Takes a 4x4 matrix (a sequence of four float4 vectors) as input."); }

  static SHTypesInfo outputTypes() { return CoreInfo::Float4x4Type; }
  static SHOptionalString outputHelp() {
    return SHCCSTR("Returns the inverse of the input 4x4 matrix.");
  }

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

  static SHOptionalString help() {
    return SHCCSTR(
        "This shard creates a 4x4 orthographic projection matrix based on the width size, height size, near, "
        "and far planes specified in the appropriate parameters. A 4x4 matrix is a sequence with exactly 4 float4 vectors while a float4 vector is a vector with 4 "
        "float elements.");
  }

  static SHTypesInfo inputTypes() { return CoreInfo::NoneType; }
  static SHOptionalString inputHelp() { return DefaultHelpText::InputHelpIgnored; }

  static SHTypesInfo outputTypes() { return CoreInfo::Float4x4Type; }
  static SHOptionalString outputHelp() {
    return SHCCSTR("Returns a 4x4 orthographic projection matrix (a sequence of four float4 vectors).");
  }

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

  static SHOptionalString help() {
    return SHCCSTR("This shard creates a 4x4 translation matrix (a sequence of four float4 vectors) from a float3 vector input "
                   "representing the translation in x, y, and z directions. A float4 vector is a vector with 4 float elements "
                   "while a float3 vector is a vector with 3 float elements.");
  }

  static SHTypesInfo inputTypes() { return CoreInfo::Float3Type; }
  static SHOptionalString inputHelp() {
    return SHCCSTR("Takes a float3 vector (a vector with 3 float elements) that represents the translation in x, y, and z "
                   "directions. The first element in the vector being x, the second y and the third z.");
  }

  static SHTypesInfo outputTypes() { return CoreInfo::Float4x4Type; }
  static SHOptionalString outputHelp() {
    return SHCCSTR("Returns a 4x4 translation matrix (a sequence of four float4 vectors).");
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    auto v3 = reinterpret_cast<const Vec3 *>(&input);
    _output = linalg::translation_matrix(**v3);
    return _output;
  }
};

struct Scaling {
  Mat4 _output{};

  static SHOptionalString help() {
    return SHCCSTR(
        "This shard creates a 4x4 scaling matrix (a sequence of four float4 vectors) from a float3 vector input that represents "
        "the scaling factors in x, y, and z directions. A float4 vector is a vector with 4 float elements while a float3 vector "
        "is a vector with 3 float elements.");
  }

  static SHTypesInfo inputTypes() { return CoreInfo::Float3Type; }
  static SHOptionalString inputHelp() {
    return SHCCSTR("Takes a float3 vector (a vector with 3 float elements) that represents the scaling factors in x, y, and z "
                   "directions. The first element in the vector being x, the second y and the third z.");
  }

  static SHTypesInfo outputTypes() { return CoreInfo::Float4x4Type; }
  static SHOptionalString outputHelp() { return SHCCSTR("Returns a 4x4 scaling matrix (a sequence of four float4 vectors)."); }

  SHVar activate(SHContext *context, const SHVar &input) {
    auto v3 = reinterpret_cast<const Vec3 *>(&input);
    _output = linalg::scaling_matrix(**v3);
    return _output;
  }
};

struct Rotation {
  Mat4 _output{};

  static SHOptionalString help() {
    return SHCCSTR("This shard creates a 4x4 rotation matrix (a sequence of four float4 vectors) from a float4 vector input "
                   "representing a rotation quaternion. A float4 vector is a vector with 4 float elements.");
  }

  static SHTypesInfo inputTypes() { return CoreInfo::Float4Type; }
  static SHOptionalString inputHelp() {
    return SHCCSTR("Takes a float4 vector (a vector with 4 float elements) representing a rotation quaternion.");
  }

  static SHTypesInfo outputTypes() { return CoreInfo::Float4x4Type; }
  static SHOptionalString outputHelp() { return SHCCSTR("Returns a 4x4 rotation matrix (a sequence of four float4 vectors)."); }

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

  static SHOptionalString help() {
    return SHCCSTR(
        "This shard creates a 4x4 view matrix (a sequence of four float4 vectors) for a camera based on the camera's "
        "position and a target point which is "
        "represented as a table with two float3 vectors: 'Position' and 'Target', that is passed as input. A float4 vector "
        "is a vector with 4 float elements while a float3 vector is a vector with 3 float elements.");
  }

  static SHTypesInfo inputTypes() { return InputTable; }
  static SHOptionalString inputHelp() {
    return SHCCSTR("Takes a table with two float3 values: 'Position' (the camera's position) and 'Target' (the point the camera "
                   "is looking at). Eg. { Position: @f3(1 2 3) Target: @f3(4 5 6) }");
  }

  static SHTypesInfo outputTypes() { return CoreInfo::Float4x4Type; }
  static SHOptionalString outputHelp() { return SHCCSTR("Returns a 4x4 view matrix (a sequence of four float4 vectors)."); }

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
  static SHOptionalString inputHelp() {
    if constexpr (AXIS == AxisX) {
      return SHCCSTR("Takes a float value representing the X rotation in radians.");
    } else if constexpr (AXIS == AxisY) {
      return SHCCSTR("Takes a float value representing the Y rotation in radians.");
    } else if constexpr (AXIS == AxisZ) {
      return SHCCSTR("Takes a float value representing the Z rotation in radians.");
    } else {
      return SHCCSTR("Takes a float value representing an angle in radians.");
    }
  }

  static SHTypesInfo outputTypes() { return CoreInfo::Float4Type; }
  static SHOptionalString outputHelp() {
    if constexpr (AXIS == AxisX) {
      return SHCCSTR(
          "Returns a float4 vector (a vector with 4 float elements) representing a rotation quaternion around the X-axis.");
    } else if constexpr (AXIS == AxisY) {
      return SHCCSTR(
          "Returns a float4 vector (a vector with 4 float elements) representing a rotation quaternion around the Y-axis.");
    } else if constexpr (AXIS == AxisZ) {
      return SHCCSTR(
          "Returns a float4 vector (a vector with 4 float elements) representing a rotation quaternion around the Z-axis.");
    } else {
      return SHCCSTR("Returns a float4 vector (a vector with 4 float elements) representing a rotation quaternion around the "
                     "specified axis.");
    }
  }

  static SHOptionalString help() {
    if constexpr (AXIS == AxisX) {
      return SHCCSTR("This shard creates a rotation quaternion for rotation around the X-axis. It takes a float "
                     "input representing the angle in radians and returns the rotation quaternion as a float4 vector. "
                     "A float4 vector is a vector with 4 float elements.");
    } else if constexpr (AXIS == AxisY) {
      return SHCCSTR("This shard creates a rotation quaternion for rotation around the Y-axis. It takes a float "
                     "input representing the angle in radians and returns the rotation quaternion as a float4 vector. "
                     "A float4 vector is a vector with 4 float elements.");
    } else if constexpr (AXIS == AxisZ) {
      return SHCCSTR("This shard creates a rotation quaternion for rotation around the Z-axis. It takes a float "
                     "input representing the angle in radians and returns the rotation quaternion as a float4 vector. "
                     "A float4 vector is a vector with 4 float elements.");
    } else {
      return SHCCSTR("This shard creates a rotation quaternion for rotation around the specified axis. It takes a float "
                     "input representing the angle in radians and returns the rotation quaternion as a float4 vector. "
                     "A float4 vector is a vector with 4 float elements.");
    }
  }

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

  static SHOptionalString help() {
    return SHCCSTR("This shard converts the input angle from degrees to radians. The conversion is done using the formula: "
                   "radians = degrees * (π / 180).");
  }

  static SHTypesInfo inputTypes() { return CoreInfo::FloatType; }
  static SHOptionalString inputHelp() { return SHCCSTR("Takes a float value representing an angle in degrees."); }
  static SHTypesInfo outputTypes() { return CoreInfo::FloatType; }
  static SHOptionalString outputHelp() { return SHCCSTR("Returns a float value representing the input angle in radians."); }

  SHVar activate(SHContext *context, const SHVar &input) { return Var(input.payload.floatValue * (PI / 180.0)); }
};

struct Rad2Deg {
  const double PI = 3.141592653589793238463;

  static SHOptionalString help() {
    return SHCCSTR("This shard converts the input angle from radians to degrees. The conversion is done using the formula: "
                   "degrees = radians * (180 / π).");
  }

  static SHTypesInfo inputTypes() { return CoreInfo::FloatType; }
  static SHOptionalString inputHelp() { return SHCCSTR("Takes a float value representing an angle in radians."); }
  static SHTypesInfo outputTypes() { return CoreInfo::FloatType; }
  static SHOptionalString outputHelp() { return SHCCSTR("Returns a float value representing the input angle in degrees."); }

  SHVar activate(SHContext *context, const SHVar &input) { return Var(input.payload.floatValue * (180.0 / PI)); }
};

struct MatIdentity {
  PARAM_VAR(_type, "Type", "The matrix row type of the corresponding matrix", {CoreInfo2::TypeEnumInfo::Type});
  PARAM_IMPL(PARAM_IMPL_FOR(_type));

  static SHOptionalString help() {
    return SHCCSTR(
        "This shard creates a standard 4x4 identity matrix. The standard identity matrix is a square matrix with 1s on the main "
        "diagonal and 0s for the other elements. A 4x4 matrix is a sequence with exactly 4 float4 vector and a float4 vector is "
        "a vector with "
        "4 float elements.");
  }
  static SHTypesInfo inputTypes() { return CoreInfo::NoneType; }
  static SHOptionalString inputHelp() { return DefaultHelpText::InputHelpIgnored; }
  static SHTypesInfo outputTypes() { return CoreInfo::Float4x4Type; }
  static SHOptionalString outputHelp() {
    return SHCCSTR(
        "Returns a 4x4 identity matrix (a sequence of four float4 vectors). The matrix will have 1s on the main diagonal "
        "and 0s for the other elements.");
  }

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
