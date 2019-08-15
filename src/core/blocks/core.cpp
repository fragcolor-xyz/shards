#include "shared.hpp"

namespace chainblocks {
struct Const {
  static inline TypesInfo constTypesInfo = TypesInfo::FromMany(
      CBType::Int, CBType::Int2, CBType::Int3, CBType::Int4, CBType::Int8,
      CBType::Int16, CBType::Float, CBType::Float2, CBType::Float3,
      CBType::Float4, CBType::None, CBType::String, CBType::Color,
      CBType::Bool);
  static inline ParamsInfo constParamsInfo = ParamsInfo(
      ParamsInfo::Param("Value", "The constant value to insert in the chain.",
                        CBTypesInfo(constTypesInfo)));

  CBVar _value{};
  TypesInfo _valueInfo;

  void destroy() { destroyVar(_value); }

  static CBTypesInfo inputTypes() { return CBTypesInfo(SharedTypes::noneInfo); }

  static CBTypesInfo outputTypes() { return CBTypesInfo(SharedTypes::anyInfo); }

  static CBParametersInfo parameters() {
    return CBParametersInfo(constParamsInfo);
  }

  void setParam(int index, CBVar value) { cloneVar(_value, value); }

  CBVar getParam(int index) { return _value; }

  CBTypeInfo inferTypes(CBTypeInfo inputType,
                        CBExposedTypesInfo consumableVariables) {
    // TODO derive info for special types like Seq, Hashes etc
    _valueInfo = TypesInfo(_value.valueType);
    return CBTypeInfo(_valueInfo);
  }

  CBVar activate(CBContext *context, CBVar input) { return _value; }
};

// Register Const
RUNTIME_CORE_BLOCK(Const);
RUNTIME_BLOCK_destroy(Const);
RUNTIME_BLOCK_inputTypes(Const);
RUNTIME_BLOCK_outputTypes(Const);
RUNTIME_BLOCK_parameters(Const);
RUNTIME_BLOCK_inferTypes(Const);
RUNTIME_BLOCK_setParam(Const);
RUNTIME_BLOCK_getParam(Const);
RUNTIME_BLOCK_activate(Const);
RUNTIME_BLOCK_END(Const);

void registerBlocksCoreBlocks() { REGISTER_CORE_BLOCK(Const); }
}; // namespace chainblocks