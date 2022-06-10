#pragma once

#include "shared.hpp"
#include <linalg_shim.hpp>
#include <math.h>

namespace shards {
namespace Math {
namespace LinAlg {
struct VectorUnaryBase : public UnaryBase {
  static SHTypesInfo inputTypes() { return CoreInfo::FloatVectors; }

  static SHTypesInfo outputTypes() { return CoreInfo::FloatVectors; }

  SHTypeInfo compose(const SHInstanceData &data) { return data.inputType; }

  template <class Operation> SHVar doActivate(SHContext *context, const SHVar &input, Operation operate) {
    if (input.valueType == Seq) {
      _result.valueType = Seq;
      shards::arrayResize(_result.payload.seqValue, 0);
      for (uint32_t i = 0; i < input.payload.seqValue.len; i++) {
        SHVar scratch;
        operate(scratch, input);
        shards::arrayPush(_result.payload.seqValue, scratch);
      }
      return _result;
    } else {
      SHVar scratch;
      operate(scratch, input);
      return scratch;
    }
  }
};

struct VectorBinaryBase : public BinaryBase {
  static SHTypesInfo inputTypes() { return CoreInfo::FloatVectors; }

  static SHTypesInfo outputTypes() { return CoreInfo::FloatVectors; }

  static inline ParamsInfo paramsInfo =
      ParamsInfo(ParamsInfo::Param("Operand", SHCCSTR("The operand."), CoreInfo::FloatVectorsOrVar));

  static SHParametersInfo parameters() { return SHParametersInfo(paramsInfo); }

  template <class Operation> SHVar doActivate(SHContext *context, const SHVar &input, Operation operate) {
    auto &operand = _operand.get();
    if (_opType == Normal) {
      SHVar scratch;
      operate(scratch, input, operand);
      return scratch;
    } else if (_opType == Seq1) {
      _result.valueType = Seq;
      shards::arrayResize(_result.payload.seqValue, 0);
      for (uint32_t i = 0; i < input.payload.seqValue.len; i++) {
        SHVar scratch;
        operate(scratch, input.payload.seqValue.elements[i], operand);
        shards::arrayPush(_result.payload.seqValue, scratch);
      }
      return _result;
    } else {
      _result.valueType = Seq;
      shards::arrayResize(_result.payload.seqValue, 0);
      for (uint32_t i = 0; i < input.payload.seqValue.len && i < operand.payload.seqValue.len; i++) {
        SHVar scratch;
        operate(scratch, input.payload.seqValue.elements[i], operand.payload.seqValue.elements[i]);
        shards::arrayPush(_result.payload.seqValue, scratch);
      }
      return _result;
    }
  }
};

struct Cross : public VectorBinaryBase {
  struct Operation {
    void operator()(SHVar &output, const SHVar &input, const SHVar &operand);
  };

  SHVar activate(SHContext *context, const SHVar &input);
};

struct Dot : public VectorBinaryBase {
  static SHTypesInfo outputTypes() { return CoreInfo::FloatType; }

  SHTypeInfo compose(const SHInstanceData &data) {
    VectorBinaryBase::compose(data);
    return CoreInfo::FloatType;
  }

  struct Operation {
    void operator()(SHVar &output, const SHVar &input, const SHVar &operand);
  };

  SHVar activate(SHContext *context, const SHVar &input);
};

struct LengthSquared : public VectorUnaryBase {
  static SHTypesInfo outputTypes() { return CoreInfo::FloatType; }

  SHTypeInfo compose(const SHInstanceData &data) {
    VectorUnaryBase::compose(data);
    return CoreInfo::FloatType;
  }

  struct Operation {
    Dot::Operation dotOp;
    void operator()(SHVar &output, const SHVar &input) { dotOp(output, input, input); }
  };
  SHVar activate(SHContext *context, const SHVar &input) {
    const Operation op;
    return doActivate(context, input, op);
  }
};

struct Length : public VectorUnaryBase {
  static SHTypesInfo outputTypes() { return CoreInfo::FloatType; }

  SHTypeInfo compose(const SHInstanceData &data) {
    VectorUnaryBase::compose(data);
    return CoreInfo::FloatType;
  }

  struct Operation {
    LengthSquared::Operation lenOp;
    void operator()(SHVar &output, const SHVar &input) {
      lenOp(output, input);
      output.payload.floatValue = __builtin_sqrt(output.payload.floatValue);
    }
  };
  SHVar activate(SHContext *context, const SHVar &input) {
    const Operation op;
    return doActivate(context, input, op);
  }
};

struct Normalize : public VectorUnaryBase {
  std::vector<SHVar> _output;
  bool _positiveOnly{false};

  // Normalize also supports Float seqs
  static SHTypesInfo inputTypes() { return CoreInfo::FloatVectorsOrFloatSeq; }
  static SHTypesInfo outputTypes() { return CoreInfo::FloatVectorsOrFloatSeq; }

  SHTypeInfo compose(const SHInstanceData &data) {
    if (data.inputType.basicType == Seq && data.inputType.seqTypes.len == 1 &&
        data.inputType.seqTypes.elements[0].basicType == Float) {
      OVERRIDE_ACTIVATE(data, activateFloatSeq);
    } else {
      OVERRIDE_ACTIVATE(data, activate);
    }
    return data.inputType;
  }

  static inline Parameters Params{
      {"Positive", SHCCSTR("If the output should be in the range 0.0~1.0 instead of -1.0~1.0."), {CoreInfo::BoolType}}};

  SHParametersInfo parameters() { return Params; }

  void setParam(int index, const SHVar &value) { _positiveOnly = value.payload.boolValue; }

  SHVar getParam(int index) { return Var(_positiveOnly); }

  struct Operation {
    bool positiveOnly;
    Length::Operation lenOp;

    void operator()(SHVar &output, const SHVar &input);
  };

  SHVar activate(SHContext *context, const SHVar &input);
  SHVar activateFloatSeq(SHContext *context, const SHVar &input);
};

struct MatMul : public VectorBinaryBase {
  // MatMul is special...
  // Mat @ Mat = Mat
  // Mat @ Vec = Vec
  // If ever becomes a bottle neck, valgrind and optimize

  SHTypeInfo compose(const SHInstanceData &data) {
    BinaryBase::compose(data);
    if (_opType == SeqSeq) {
      return data.inputType;
    } else {
      if (data.inputType.seqTypes.len != 1) {
        throw SHException("MatMul expected a unique Seq inner type.");
      }
      return data.inputType.seqTypes.elements[0];
    }
  }

  SHVar activate(SHContext *context, const SHVar &input);
};

struct Transpose : public VectorUnaryBase {
  SHTypeInfo compose(const SHInstanceData &data) {
    if (data.inputType.basicType != Seq) {
      throw ComposeError("Transpose expected a Seq matrix array as input.");
    }
    return data.inputType;
  }

  SHVar activate(SHContext *context, const SHVar &input);
};

struct Orthographic : VectorUnaryBase {
  double _width = 1280;
  double _height = 720;
  double _near = 0.0;
  double _far = 1000.0;

  void setup() {
    _result.valueType = Seq;
    shards::arrayResize(_result.payload.seqValue, 4);
    for (auto i = 0; i < 4; i++) {
      _result.payload.seqValue.elements[i] = Var::Empty;
      _result.payload.seqValue.elements[i].valueType = Float4;
    }
  }

  SHTypeInfo compose(const SHInstanceData &data) { return CoreInfo::Float4SeqType; }

  static SHTypesInfo inputTypes() { return CoreInfo::NoneType; }
  static SHTypesInfo outputTypes() { return CoreInfo::Float4SeqType; }

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
    _result.payload.seqValue.elements[0].payload.float4Value[0] = 2.0 / (right - left);
    _result.payload.seqValue.elements[1].payload.float4Value[1] = 2.0 / (top - bottom);
    _result.payload.seqValue.elements[2].payload.float4Value[2] = -zrange;
    _result.payload.seqValue.elements[3].payload.float4Value[0] = (left + right) / (left - right);
    _result.payload.seqValue.elements[3].payload.float4Value[1] = (top + bottom) / (bottom - top);
    _result.payload.seqValue.elements[3].payload.float4Value[2] = -_near * zrange;
    _result.payload.seqValue.elements[3].payload.float4Value[3] = 1.0;
    return _result;
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
    _output = linalg::translation_matrix(*v3);
    return _output;
  }
};

struct Scaling {
  Mat4 _output{};

  static SHTypesInfo inputTypes() { return CoreInfo::Float3Type; }
  static SHTypesInfo outputTypes() { return CoreInfo::Float4x4Type; }

  SHVar activate(SHContext *context, const SHVar &input) {
    auto v3 = reinterpret_cast<const Vec3 *>(&input);
    _output = linalg::scaling_matrix(*v3);
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
    _output = linalg::rotation_matrix(*v4);
    return _output;
  }
};

struct LookAt {
  static inline Types InputTableTypes{{CoreInfo::Float3Type, CoreInfo::Float3Type}};
  static inline std::array<SHString, 2> InputTableKeys{"Position", "Target"};
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
      SHString k;
      SHVar v;
      if (!table.api->tableNext(table, &it, &k, &v))
        break;

      switch (k[0]) {
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

    auto eye = reinterpret_cast<const Vec3 *>(&position);
    auto center = reinterpret_cast<const Vec3 *>(&target);
    _output = linalg::lookat_matrix(*eye, *center, {0.0, 1.0, 0.0});
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

} // namespace LinAlg
} // namespace Math
} // namespace shards
