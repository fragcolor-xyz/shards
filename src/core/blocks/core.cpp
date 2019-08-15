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
  std::string _key;
  ExposedInfo _exposedInfo{};
  bool _isTable = false;

  static inline ParamsInfo variableParamsInfo = ParamsInfo(
      ParamsInfo::Param("Name", "The name of the variable.",
                        CBTypesInfo(SharedTypes::strInfo)),
      ParamsInfo::Param("Key",
                        "The key of the value to read/write from/in the table "
                        "(this variable will become a table).",
                        CBTypesInfo(SharedTypes::strInfo)));

  static CBParametersInfo parameters() {
    return CBParametersInfo(variableParamsInfo);
  }

  void setParam(int index, CBVar value) {
    if (index == 0)
      _name = value.payload.stringValue;
    else if (index == 1) {
      _key = value.payload.stringValue;
      if (_key.size() > 0)
        _isTable = true;
      else
        _isTable = false;
    }
  }

  CBVar getParam(int index) {
    if (index == 0)
      return Var(_name.c_str());
    else if (index == 1)
      return Var(_key.c_str());
    throw CBException("Param index out of range.");
  }
};

struct Set : public VariableBase {
  CBVar _tableRecord{};
  TypesInfo _tableTypeInfo{};
  TypesInfo _tableContentInfo{};

  void cleanup() {
    if (_target) {
      if (_isTable && _target->valueType == Table) {
        // Destroy the value we are holding..
        destroyVar(_tableRecord);
        // Remove from table
        stbds_shdel(_target->payload.tableValue, _key.c_str());
        // Finally free the table if has no values
        if (stbds_shlen(_target->payload.tableValue) == 0) {
          stbds_shfree(_target->payload.tableValue);
          *_target = CBVar();
        }
      } else {
        destroyVar(*_target);
      }
    }
    _target = nullptr;
  }

  static CBTypesInfo inputTypes() { return CBTypesInfo(SharedTypes::anyInfo); }

  static CBTypesInfo outputTypes() { return CBTypesInfo(SharedTypes::anyInfo); }

  CBTypeInfo inferTypes(CBTypeInfo inputType,
                        CBExposedTypesInfo consumableVariables) {
    // bake exposed types
    if (_isTable) {
      // we are a table!
      _tableContentInfo = TypesInfo(inputType);
      _tableTypeInfo = TypesInfo(CBType::Table, CBTypesInfo(_tableContentInfo),
                                 _key.c_str());
      _exposedInfo = ExposedInfo(ExposedInfo::Variable(
          _name.c_str(), "The exposed table.", CBTypeInfo(_tableTypeInfo)));
    } else {
      // just a variable!
      _exposedInfo = ExposedInfo(ExposedInfo::Variable(
          _name.c_str(), "The exposed variable.", CBTypeInfo(inputType)));
    }
    return inputType;
  }

  CBExposedTypesInfo exposedVariables() {
    return CBExposedTypesInfo(_exposedInfo);
  }

  CBVar activate(CBContext *context, CBVar input) {
    if (!_target) {
      _target = contextVariable(context, _name.c_str());
    }
    if (_isTable) {
      if (_target->valueType != Table) {
        // Not initialized yet
        _target->valueType = Table;
        _target->payload.tableValue = nullptr;
        // Also set table default
        CBVar defaultVar{};
        defaultVar.valueType = None;
        defaultVar.payload.chainState = Continue;
        CBNamedVar defaultNamed = {nullptr, defaultVar};
        stbds_shdefaults(_target->payload.tableValue, defaultNamed);
      }

      // Get the current value, if not in it will be default None anyway
      // If was in we actually clone on top of it so we recycle memory
      _tableRecord = stbds_shget(_target->payload.tableValue, _key.c_str());
      cloneVar(_tableRecord, input);
      stbds_shput(_target->payload.tableValue, _key.c_str(), _tableRecord);
    } else {
      // Clone will try to recyle memory and such
      cloneVar(*_target, input);
    }
    return input;
  }
};

struct Get : public VariableBase {
  void cleanup() { _target = nullptr; }

  static CBTypesInfo inputTypes() { return CBTypesInfo(SharedTypes::noneInfo); }

  static CBTypesInfo outputTypes() { return CBTypesInfo(SharedTypes::anyInfo); }

  CBTypeInfo inferTypes(CBTypeInfo inputType,
                        CBExposedTypesInfo consumableVariables) {
    if (_isTable) {
      for (auto i = 0; stbds_arrlen(consumableVariables) > i; i++) {
        if (consumableVariables[i].name == _name &&
            consumableVariables[i].exposedType.tableTypes) {
          auto &tableKeys = consumableVariables[i].exposedType.tableKeys;
          auto &tableTypes = consumableVariables[i].exposedType.tableTypes;
          for (auto y = 0; y < stbds_arrlen(tableKeys); y++) {
            if (_key == tableKeys[y]) {
              return tableTypes[y];
            }
          }
          throw CBException(
              "Get: Could not infer an output type, key not found.");
        }
      }
    } else {
      for (auto i = 0; i < stbds_arrlen(consumableVariables); i++) {
        auto &cv = consumableVariables[i];
        if (_name == cv.name) {
          return cv.exposedType;
        }
      }
    }
    throw CBException("Get: Could not infer an output type.");
  }

  CBExposedTypesInfo consumedVariables() {
    if (_isTable) {
      _exposedInfo = ExposedInfo(
          ExposedInfo::Variable(_name.c_str(), "The consumed table.",
                                CBTypeInfo(SharedTypes::tableInfo)));
    } else {
      _exposedInfo = ExposedInfo(
          ExposedInfo::Variable(_name.c_str(), "The consumed variable.",
                                CBTypeInfo(SharedTypes::anyInfo)));
    }
    return CBExposedTypesInfo(_exposedInfo);
  }

  CBVar activate(CBContext *context, CBVar input) {
    if (!_target) {
      _target = contextVariable(context, _name.c_str());
    }
    if (_isTable) {
      if (_target->valueType == Table) {
        // TODO this might return None (default)
        return stbds_shget(_target->payload.tableValue, _key.c_str());
      } else {
        // TODO should we throw?
        return Var::Restart();
      }
    } else {
      return *_target;
    }
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

// Register Set
RUNTIME_CORE_BLOCK(Set);
RUNTIME_BLOCK_cleanup(Set);
RUNTIME_BLOCK_inputTypes(Set);
RUNTIME_BLOCK_outputTypes(Set);
RUNTIME_BLOCK_parameters(Set);
RUNTIME_BLOCK_inferTypes(Set);
RUNTIME_BLOCK_exposedVariables(Set);
RUNTIME_BLOCK_setParam(Set);
RUNTIME_BLOCK_getParam(Set);
RUNTIME_BLOCK_activate(Set);
RUNTIME_BLOCK_END(Set);

// Register Get
RUNTIME_CORE_BLOCK(Get);
RUNTIME_BLOCK_cleanup(Get);
RUNTIME_BLOCK_inputTypes(Get);
RUNTIME_BLOCK_outputTypes(Get);
RUNTIME_BLOCK_parameters(Get);
RUNTIME_BLOCK_inferTypes(Get);
RUNTIME_BLOCK_consumedVariables(Get);
RUNTIME_BLOCK_setParam(Get);
RUNTIME_BLOCK_getParam(Get);
RUNTIME_BLOCK_activate(Get);
RUNTIME_BLOCK_END(Get);

void registerBlocksCoreBlocks() {
  REGISTER_CORE_BLOCK(Const);
  REGISTER_CORE_BLOCK(Set);
  REGISTER_CORE_BLOCK(Get);
}
}; // namespace chainblocks