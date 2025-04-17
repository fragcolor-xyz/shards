#include <shards/core/params.hpp>
#include <shards/core/compose.hpp>
#include <shards/common_types.hpp>

namespace shards {
struct Set {
  PARAM_PARAMVAR(_name, "Name", "The name of the variable to set.", {CoreInfo::StringOrAnyVar});
  PARAM_VAR(_global, "Global", "If the variable is global.", {CoreInfo::BoolType});
  PARAM_IMPL(PARAM_IMPL_FOR(_name), PARAM_IMPL_FOR(_global));

  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return CoreInfo::AnyType; }

  ExposedInfo _exposed;
  SHVar *_slot{};

  Set() {
    _name = Var("");
    _global = Var(false);
  }

  SHExposedTypesInfo exposedVariables() { return SHExposedTypesInfo(_exposed); }

  SHTypeInfo compose(SHInstanceData &data) {
    _exposed.push_back(SHExposedTypeInfo{
        .name = _name->payload.stringValue,
        .exposedType = data.inputType,
        .isMutable = true,
        .global = _global->payload.boolValue,
        .declared = true,
    });
    return data.inputType;
  }

  void warmup(SHContext *ctx) {
    if (_global->payload.boolValue) {
      _slot = referenceGlobalVariable(ctx, _name->payload.stringValue);
    } else {
      _slot = referenceVariable(ctx, _name->payload.stringValue);
    }
  }

  void cleanup(SHContext *ctx) { releaseVariable(_slot); }

  SHVar activate(SHContext *ctx, const SHVar &input) {
    (*_slot) = input;
    return input;
  }
};

struct Ref {
  PARAM_PARAMVAR(_name, "Name", "The name of the variable to set.", {CoreInfo::StringOrAnyVar});
  PARAM_VAR(_global, "Global", "If the variable is global.", {CoreInfo::BoolType});
  PARAM_IMPL(PARAM_IMPL_FOR(_name), PARAM_IMPL_FOR(_global));

  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return CoreInfo::AnyType; }

  ExposedInfo _exposed;
  SHVar *_slot{};

  Ref() {
    _name = Var("");
    _global = Var(false);
  }

  SHExposedTypesInfo exposedVariables() { return SHExposedTypesInfo(_exposed); }

  SHTypeInfo compose(SHInstanceData &data) {
    _exposed.push_back(SHExposedTypeInfo{
        .name = _name->payload.stringValue,
        .exposedType = data.inputType,
        .isMutable = false,
        .global = _global->payload.boolValue,
        .declared = true,
    });
    return data.inputType;
  }

  SHVar activate(SHContext *ctx, const SHVar &input) { return input; }
};

struct Update {
  PARAM_PARAMVAR(_name, "Name", "The name of the variable to update.", {CoreInfo::StringOrAnyVar});
  PARAM_VAR(_global, "Global", "If the variable is global.", {CoreInfo::BoolType});
  PARAM_IMPL(PARAM_IMPL_FOR(_name), PARAM_IMPL_FOR(_global));

  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return CoreInfo::AnyType; }

  SHVar *_slot{};
  bool _existingDeclaredAsGlobal{};

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(SHInstanceData &data) {
    _requiredVariables.clear();
    auto &ctx = compose::CompositionContext::get(data);
    auto existing = ctx.findVariable(_name->payload.stringValue);
    if (!existing)
      throw ComposeError(fmt::format("Variable {} not found", _name->payload.stringValue));
    _requiredVariables.push_back(existing->exposed);
    _existingDeclaredAsGlobal = existing->kind == compose::VariableKind::Global;
    return data.inputType;
  }

  void warmup(SHContext *ctx) {
    if (_existingDeclaredAsGlobal || _global->payload.boolValue) {
      _slot = referenceGlobalVariable(ctx, _name->payload.stringValue);
    } else {
      _slot = referenceVariable(ctx, _name->payload.stringValue);
    }
  }

  void cleanup(SHContext *ctx) { releaseVariable(_slot); }

  SHVar activate(SHContext *ctx, const SHVar &input) { return input; }
};

struct Get {
  PARAM_PARAMVAR(_name, "Name", "The name of the variable to get.", {CoreInfo::StringOrAnyVar});
  PARAM_IMPL(PARAM_IMPL_FOR(_name));

  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return CoreInfo::AnyType; }

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo composeV2(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    auto &ctx = compose::CompositionContext::get(data);
    auto var = ctx.findVariable(_name->payload.stringValue);
    if (!var) {
      throw ComposeError(fmt::format("Variable {} not found", _name->payload.stringValue));
    }
    return var->exposed.exposedType;
  }

  SHVar activate(SHContext *ctx, const SHVar &input) { return input; }
};

void registerCore2() {
  REGISTER_SHARD("Set", Set);
  REGISTER_SHARD("Update", Update);
  REGISTER_SHARD("Ref", Ref);
  REGISTER_SHARD("Get", Get);
}
} // namespace shards