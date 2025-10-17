#ifndef CFB9369D_F72D_4EA0_BD57_F57DF65999C2
#define CFB9369D_F72D_4EA0_BD57_F57DF65999C2

#include <shards/core/shared.hpp>
#include <shards/core/params.hpp>
#include <shards/common_types.hpp>

namespace shards {
struct Memoize {
  static SHTypesInfo inputTypes() { return shards::CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return shards::CoreInfo::AnyType; }
  static SHOptionalString help() { return SHCCSTR("Computes a value"); }

  PARAM(ShardsVar, _evaluate, "Evaluate", "The shards to evaluate the cached value based on input", {shards::CoreInfo::Shards});
  PARAM_IMPL(PARAM_IMPL_FOR(_evaluate));

  OwnedVar _lastInput;
  SHVar _lastOutput{};

  void warmup(SHContext *context) {
    PARAM_WARMUP(context);
    _lastInput = Var::Empty;
  }
  void cleanup(SHContext *context) { PARAM_CLEANUP(context); }

  SHTypeInfo compose(SHInstanceData &data) {
    SHComposeResult res = _evaluate.compose(data);
    if (res.failed)
      throw std::runtime_error("Failed to compose Memoize evaluate expression");
    return res.outputType;
  }

  SHExposedTypesInfo requiredVariables() { return _evaluate.composeResult().requiredInfo; }
  SHExposedTypesInfo exposedVariables() { return _evaluate.composeResult().exposedInfo; }

  SHVar activate(SHContext *shContext, const SHVar &input) {
    if (_lastInput != input) {
      _lastInput = input;
      _evaluate.activate(shContext, input, _lastOutput);
    }
    return _lastOutput;
  }
};

struct Track {
  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return CoreInfo::AnyType; }
  static SHOptionalString help() { return SHCCSTR("Tracks the variables and executes the action when they change."); }

  PARAM_VAR(_variables, "Variables", "A single variable or a sequence of variables to track for changes.", {CoreInfo::AnyType});
  PARAM(ShardsVar, _action, "Action", "The action to execute when the variables change.", {CoreInfo::Shards});
  PARAM_VAR(_mask, "Mask", "The mask to use to determine which variables to track.", {CoreInfo::IntOrNone});
  PARAM_IMPL(PARAM_IMPL_FOR(_variables), PARAM_IMPL_FOR(_action), PARAM_IMPL_FOR(_mask));

  SHTypeInfo compose(SHInstanceData &data) {
    auto res = _action.compose(data);
    if (res.failed)
      throw shards::Error("Failed to compose Track action");
    return res.outputType;
  }

  std::unordered_set<std::string_view> _varNames;
  bool _shouldActivate = false;

  SHExposedTypesInfo requiredVariables() { return _action.composeResult().requiredInfo; }
  SHExposedTypesInfo exposedVariables() { return _action.composeResult().exposedInfo; }

  void cleanup(SHContext *context) {
    _action.cleanup(context);

    _varNames.clear();

    if (_connection) {
      _connection.release();
    }
  }

  entt::scoped_connection _connection;
  SHWire *_triggerWire{nullptr};

  void warmup(SHContext *context) {
    _action.warmup(context);

    if (_variables.valueType == SHType::Seq) {
      for (auto &variable : _variables) {
        if (variable.valueType != SHType::ContextVar) {
          throw WarmupError("Track variables must be context variables");
        }
        auto name = SHSTRVIEW(variable);
        _varNames.insert(name);
      }
    } else {
      if (_variables.valueType != SHType::ContextVar) {
        throw WarmupError("Track variables must be context variables");
      }
      auto name = SHSTRVIEW(_variables);
      _varNames.insert(name);
    }

    auto mesh = context->rootWire()->mesh.lock();
    if (_mask->isNone()) {
      _connection = mesh->dispatcher.sink<shards::OnTrackedVarSet>().connect<&Track::handleTrackedVarSet>(this);
    } else {
      _connection = mesh->dispatcher.sink<shards::OnTrackedVarSet>().connect<&Track::handleTrackedVarSetWithMask>(this);
    }

    _triggerWire = context->rootWire();

    _shouldActivate = true; // always trigger the first time
  }

  void handleTrackedVarSet(OnTrackedVarSet &event) {
    shassert(event.wire && event.wire->context && event.wire->context->rootWire() &&
             "Tracked var set event should have a valid wire");
    if (!_shouldActivate && event.wire->context->rootWire() == _triggerWire && _varNames.contains(event.name)) {
      _shouldActivate = true;
    }
  }

  void handleTrackedVarSetWithMask(OnTrackedVarSet &event) {
    shassert(event.wire && event.wire->context && event.wire->context->rootWire() &&
             "Tracked var set event should have a valid wire");
    if (!_shouldActivate && event.wire->context->rootWire() == _triggerWire && _varNames.contains(event.name) &&
        (event.newValue.trackingMask & _mask.payload.intValue) != 0) {
      _shouldActivate = true;
    }
  }

  SHVar _lastOutput{};

  SHVar &activate(SHContext *context, const SHVar &input) {
    if (_shouldActivate) {
      _shouldActivate = false;

      _action.activate(context, input, _lastOutput);
    }

    return _lastOutput;
  }
};

struct Trigger {
  // a shard that will wait for a bool variable to be true or a sequence of bool variables to be all true
  // and then trigger the action
  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return CoreInfo::AnyType; }
  static SHOptionalString help() { return SHCCSTR("Triggers the action when the variables are true"); }

  PARAM_PARAMVAR(_variables, "Variables", "A single variable or a sequence of variables to track for changes.",
                 {CoreInfo::BoolVarType});
  PARAM(ShardsVar, _action, "Action", "The action to execute when the variables change.", {CoreInfo::Shards});
  PARAM_IMPL(PARAM_IMPL_FOR(_variables), PARAM_IMPL_FOR(_action));

  SHTypeInfo compose(SHInstanceData &data) {
    auto res = _action.compose(data);
    if (res.failed)
      throw shards::Error("Failed to compose Trigger action");
    return data.inputType;
  }

  SHExposedTypesInfo requiredVariables() { return _action.composeResult().requiredInfo; }

  void cleanup(SHContext *context) { PARAM_CLEANUP(context); }

  void warmup(SHContext *context) { PARAM_WARMUP(context); }

  void activate(SHContext *context, const SHVar &input) {
    auto &vars = _variables.get();
    bool trigger = true;
    if (vars.valueType == SHType::Seq) {
      for (auto &variable : vars) {
        trigger = trigger && variable.payload.boolValue;
        variable.payload.boolValue = false; // reset the variable
      }
    } else {
      trigger = vars.payload.boolValue;
      vars.payload.boolValue = false; // reset the variable
    }

    if (trigger) {
      SHVar output{};
      _action.activate(context, input, output);
    }
  }
};
} // namespace shards

#endif /* CFB9369D_F72D_4EA0_BD57_F57DF65999C2 */
