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

    auto &ctx = CompositionContext::get(data);

    // Don't allow references to escape the Once block
    ctx.invalidateExposedReferences(_evaluate.composeResult().exposedInfo);

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

  PARAM_VAR(_variables, "Variables", "The variables to track for changes.", {CoreInfo::AnySeqType});
  PARAM(ShardsVar, _action, "Action", "The action to execute when the variables change.", {CoreInfo::ShardsOrNoneSeq});
  PARAM_IMPL(PARAM_IMPL_FOR(_variables), PARAM_IMPL_FOR(_action));

  SHTypeInfo compose(SHInstanceData &data) {
    auto res = _action.compose(data);
    if (res.failed)
      throw ComposeError("Failed to compose Track action");
    return res.outputType;
  }

  std::unordered_set<std::string_view> _varNames;
  bool _shouldActivate = false;

  SHExposedTypesInfo requiredVariables() { return _action.composeResult().requiredInfo; }
  SHExposedTypesInfo exposedVariables() { return _action.composeResult().exposedInfo; }

  void cleanup(SHContext *context) {
    _action.cleanup(context);

    _varNames.clear();

    if (context) {
      auto mesh = context->rootWire()->mesh.lock();
      if (mesh) {
        mesh->dispatcher.sink<shards::OnTrackedVarSet>().disconnect<&Track::handleTrackedVarSet>(this);
      }
    }
  }

  void warmup(SHContext *context) {
    _action.warmup(context);

    for (auto &variable : _variables) {
      auto name = SHSTRVIEW(variable);
      _varNames.insert(name);
    }

    auto mesh = context->rootWire()->mesh.lock();
    mesh->dispatcher.sink<shards::OnTrackedVarSet>().connect<&Track::handleTrackedVarSet>(this);

    _shouldActivate = true; // always trigger the first time
  }

  void handleTrackedVarSet(OnTrackedVarSet &event) {
    if (!_shouldActivate && _varNames.contains(event.name)) {
      _shouldActivate = true;
    }
  }

  OwnedVar _output;

  SHVar &activate(SHContext *context, const SHVar &input) {
    if (_shouldActivate) {
      _shouldActivate = false;

      _action.activate(context, input, _output);
    }

    return _output;
  }
};
} // namespace shards

#endif /* CFB9369D_F72D_4EA0_BD57_F57DF65999C2 */
