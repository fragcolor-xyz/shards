#include "variables.hpp"

namespace shards::variables {

// Set implementation
SHTypesInfo Set::inputTypes() { return CoreInfo::AnyType; }
SHTypesInfo Set::outputTypes() { return CoreInfo::AnyType; }

Set::Set() {
  _name = Var("");
  _global = Var(false);
}

SHExposedTypesInfo Set::exposedVariables() { return SHExposedTypesInfo(_exposed); }

SHTypeInfo Set::compose(SHInstanceData &data) {
  _exposed.push_back(SHExposedTypeInfo{
      .name = _name->payload.stringValue,
      .exposedType = data.inputType,
      .isMutable = true,
      .global = _global->payload.boolValue,
      .declared = true,
  });
  return data.inputType;
}

void Set::warmup(SHContext *ctx) {
  if (_global->payload.boolValue) {
    _slot = referenceGlobalVariable(ctx, _name->payload.stringValue);
  } else {
    _slot = referenceVariable(ctx, _name->payload.stringValue);
  }
}

void Set::cleanup(SHContext *ctx) { 
  releaseVariableRef(_slot); 
}

SHVar Set::activate(SHContext *ctx, const SHVar &input) {
  cloneVar((*_slot), input);
  return input;
}

// Ref implementation
SHTypesInfo Ref::inputTypes() { return CoreInfo::AnyType; }
SHTypesInfo Ref::outputTypes() { return CoreInfo::AnyType; }

Ref::Ref() {
  _name = Var("");
  _global = Var(false);
}

SHExposedTypesInfo Ref::exposedVariables() { return SHExposedTypesInfo(_exposed); }

SHTypeInfo Ref::compose(SHInstanceData &data) {
  _exposed.push_back(SHExposedTypeInfo{
      .name = _name->payload.stringValue,
      .exposedType = data.inputType,
      .isMutable = false,
      .global = _global->payload.boolValue,
      .declared = true,
  });
  return data.inputType;
}

SHVar Ref::activate(SHContext *ctx, const SHVar &input) { return input; }

// Update implementation
SHTypesInfo Update::inputTypes() { return CoreInfo::AnyType; }
SHTypesInfo Update::outputTypes() { return CoreInfo::AnyType; }

SHTypeInfo Update::compose(SHInstanceData &data) {
  _requiredVariables.clear();
  auto &ctx = compose::CompositionContext::get(data);
  auto existing = ctx.findVariable(_name->payload.stringValue);
  if (!existing)
    throw ComposeError(fmt::format("Variable {} not found", _name->payload.stringValue));
  _requiredVariables.push_back(existing->exposed);
  _existingDeclaredAsGlobal = existing->kind == compose::VariableKind::Global;
  if (!matchTypes(data.inputType, existing->exposed.exposedType, true, true, true)) {
    throw ComposeError(fmt::format("Variable {} type mismatch, can not assign {} to {}", _name->payload.stringValue,
                                  data.inputType, existing->exposed.exposedType));
  }
  return data.inputType;
}

void Update::warmup(SHContext *ctx) {
  if (_existingDeclaredAsGlobal || _global->payload.boolValue) {
    _slot = referenceGlobalVariable(ctx, _name->payload.stringValue);
  } else {
    _slot = referenceVariable(ctx, _name->payload.stringValue);
  }
}

void Update::cleanup(SHContext *ctx) { releaseVariableRef(_slot); }

SHVar Update::activate(SHContext *ctx, const SHVar &input) {
  cloneVar(*_slot, input);
  return input;
}

// Get implementation
SHTypesInfo Get::inputTypes() { return CoreInfo::AnyType; }
SHTypesInfo Get::outputTypes() { return CoreInfo::AnyType; }

void Get::warmup(SHContext *ctx) { _slot = referenceVariable(ctx, _name->payload.stringValue); }
void Get::cleanup(SHContext *ctx) { releaseVariableRef(_slot); }

SHTypeInfo Get::composeV2(SHInstanceData &data) {
  PARAM_COMPOSE_REQUIRED_VARIABLES(data);
  auto &ctx = compose::CompositionContext::get(data);
  auto var = ctx.findVariable(_name->payload.stringValue);
  if (!var) {
    throw ComposeError(fmt::format("Variable {} not found", _name->payload.stringValue));
  }
  return var->exposed.exposedType;
}

SHVar Get::activate(SHContext *ctx, const SHVar &input) { return *_slot; }

void registerCore2() {
  // REGISTER_SHARD("Set", Set);
  // REGISTER_SHARD("Update", Update);
  // REGISTER_SHARD("Ref", Ref);
  // REGISTER_SHARD("Get", Get);
}
} // namespace shards