#pragma once

#ifndef CHAINBLOCKS_RUNTIME
#define CHAINBLOCKS_RUNTIME 1
#endif

#include "../runtime.hpp"

namespace chainblocks {
struct CoreInfo {
  static inline TypesInfo strInfo = TypesInfo(CBType::String);
  static inline TypesInfo anyInfo = TypesInfo(CBType::Any);
  static inline TypesInfo noneInfo = TypesInfo(CBType::None);
  static inline TypesInfo tableInfo = TypesInfo(CBType::Table);
  static inline TypesInfo floatInfo = TypesInfo(CBType::Float);
};

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

  static CBTypesInfo inputTypes() { return CBTypesInfo(CoreInfo::noneInfo); }

  static CBTypesInfo outputTypes() { return CBTypesInfo(CoreInfo::anyInfo); }

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

struct Sleep {
  static inline ParamsInfo sleepParamsInfo = ParamsInfo(ParamsInfo::Param(
      "Time", "The amount of time in seconds (float) to pause this chain.",
      CBTypesInfo(CoreInfo::floatInfo)));

  double time{};

  static CBTypesInfo inputTypes() { return CBTypesInfo(CoreInfo::anyInfo); }

  static CBTypesInfo outputTypes() { return CBTypesInfo(CoreInfo::anyInfo); }

  static CBParametersInfo parameters() {
    return CBParametersInfo(sleepParamsInfo);
  }

  void setParam(int index, CBVar value) { time = value.payload.floatValue; }

  CBVar getParam(int index) { return Var(time); }

  CBVar activate(CBContext *context, CBVar input) {
    cbpause(time);
    return input;
  }
};

struct VariableBase {
  CBVar *_target = nullptr;
  std::string _name;
  std::string _key;
  ExposedInfo _exposedInfo{};
  bool _isTable = false;

  static inline ParamsInfo variableParamsInfo = ParamsInfo(
      ParamsInfo::Param("Name", "The name of the variable.",
                        CBTypesInfo(CoreInfo::strInfo)),
      ParamsInfo::Param("Key",
                        "The key of the value to read/write from/in the table "
                        "(this variable will become a table).",
                        CBTypesInfo(CoreInfo::strInfo)));

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

  static CBTypesInfo inputTypes() { return CBTypesInfo(CoreInfo::anyInfo); }

  static CBTypesInfo outputTypes() { return CBTypesInfo(CoreInfo::anyInfo); }

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

  static CBTypesInfo inputTypes() { return CBTypesInfo(CoreInfo::noneInfo); }

  static CBTypesInfo outputTypes() { return CBTypesInfo(CoreInfo::anyInfo); }

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
        }
      }
      throw CBException("Get: Could not infer an output type, key not found.");
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
                                CBTypeInfo(CoreInfo::tableInfo)));
    } else {
      _exposedInfo = ExposedInfo(
          ExposedInfo::Variable(_name.c_str(), "The consumed variable.",
                                CBTypeInfo(CoreInfo::anyInfo)));
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

RUNTIME_CORE_BLOCK_TYPE(Const);
RUNTIME_CORE_BLOCK_TYPE(Sleep);
RUNTIME_CORE_BLOCK_TYPE(Set);
RUNTIME_CORE_BLOCK_TYPE(Get);
}; // namespace chainblocks