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

struct VariableBase {
  CBVar *_target = nullptr;
  std::string _name;

  static inline ParamsInfo variableParamsInfo = ParamsInfo(ParamsInfo::Param(
      "Name", "The name of the variable.", CBTypesInfo(SharedTypes::strInfo)));

  static CBParametersInfo parameters() {
    return CBParametersInfo(variableParamsInfo);
  }

  void setParam(int index, CBVar value) { _name = value.payload.stringValue; }

  CBVar getParam(int index) { return Var(_name.c_str()); }
};

struct SetVariable : public VariableBase {
  ExposedInfo _exposedInfo;

  void cleanup() {
    if (_target) {
      destroyVar(*_target);
    }
    _target = nullptr;
  }

  static CBTypesInfo inputTypes() { return CBTypesInfo(SharedTypes::anyInfo); }

  static CBTypesInfo outputTypes() { return CBTypesInfo(SharedTypes::anyInfo); }

  CBTypeInfo inferTypes(CBTypeInfo inputType,
                        CBExposedTypesInfo consumableVariables) {
    // bake exposed types
    _exposedInfo = ExposedInfo(ExposedInfo::Variable(
        _name.c_str(), "The exposed variable.", CBTypeInfo(inputType)));
    return inputType;
  }

  CBExposedTypesInfo exposedVariables() {
    return CBExposedTypesInfo(_exposedInfo);
  }

  CBVar activate(CBContext *context, CBVar input) {
    if (!_target) {
      _target = contextVariable(context, _name.c_str());
    }
    // Clone will try to recyle memory and such
    cloneVar(*_target, input);
    return input;
  }
};

struct GetVariable : public VariableBase {
  ExposedInfo _exposedInfo;

  void cleanup() { _target = nullptr; }

  static CBTypesInfo inputTypes() { return CBTypesInfo(SharedTypes::noneInfo); }

  static CBTypesInfo outputTypes() { return CBTypesInfo(SharedTypes::anyInfo); }

  CBTypeInfo inferTypes(CBTypeInfo inputType,
                        CBExposedTypesInfo consumableVariables) {
    for (auto i = 0; i < stbds_arrlen(consumableVariables); i++) {
      auto &cv = consumableVariables[i];
      if (_name == cv.name) {
        return cv.exposedType;
      }
    }
    throw CBException("GetVariable: Could not infer an output type.");
  }

  CBExposedTypesInfo consumedVariables() {
    _exposedInfo = ExposedInfo(
        ExposedInfo::Variable(_name.c_str(), "The consumed variable.",
                              CBTypeInfo(SharedTypes::anyInfo)));
    return CBExposedTypesInfo(_exposedInfo);
  }

  CBVar activate(CBContext *context, CBVar input) {
    if (!_target) {
      _target = contextVariable(context, _name.c_str());
    }
    return *_target;
  }
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

// Register SetVariable
RUNTIME_CORE_BLOCK(SetVariable);
RUNTIME_BLOCK_cleanup(SetVariable);
RUNTIME_BLOCK_inputTypes(SetVariable);
RUNTIME_BLOCK_outputTypes(SetVariable);
RUNTIME_BLOCK_parameters(SetVariable);
RUNTIME_BLOCK_inferTypes(SetVariable);
RUNTIME_BLOCK_exposedVariables(SetVariable);
RUNTIME_BLOCK_setParam(SetVariable);
RUNTIME_BLOCK_getParam(SetVariable);
RUNTIME_BLOCK_activate(SetVariable);
RUNTIME_BLOCK_END(SetVariable);

// Register GetVariable
RUNTIME_CORE_BLOCK(GetVariable);
RUNTIME_BLOCK_cleanup(GetVariable);
RUNTIME_BLOCK_inputTypes(GetVariable);
RUNTIME_BLOCK_outputTypes(GetVariable);
RUNTIME_BLOCK_parameters(GetVariable);
RUNTIME_BLOCK_inferTypes(GetVariable);
RUNTIME_BLOCK_consumedVariables(GetVariable);
RUNTIME_BLOCK_setParam(GetVariable);
RUNTIME_BLOCK_getParam(GetVariable);
RUNTIME_BLOCK_activate(GetVariable);
RUNTIME_BLOCK_END(GetVariable);

void registerBlocksCoreBlocks() {
  REGISTER_CORE_BLOCK(Const);
  REGISTER_CORE_BLOCK(SetVariable);
  REGISTER_CORE_BLOCK(GetVariable);
}
}; // namespace chainblocks