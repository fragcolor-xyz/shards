#include "compose.hpp"
#include "pmr/unordered_set.hpp"
#include "pmr/vector.hpp"
#include "pmr/string.hpp"
#include "pmr/set.hpp"
#include "pmr/map.hpp"
#include "type_cache.hpp"
#include "compose_fmt.hpp"
#include <functional>
#include <shards/common_types.hpp>
#include <tracy/Wrapper.hpp>
#include <shards/log/log.hpp>

namespace shards {

namespace compose {
std::shared_ptr<spdlog::logger> logger = logging::getOrCreate("compose");
}

bool matchTypes(const SHTypeInfo &inputType, const SHTypeInfo &receiverType, bool isParameter, bool strict,
                bool relaxEmptySeqCheck, bool ignoreFixedSeq) {
  return TypeMatcher{
      .isParameter = isParameter, .strict = strict, .relaxEmptySeqCheck = relaxEmptySeqCheck, .ignoreFixedSeq = ignoreFixedSeq}
      .match(inputType, receiverType);
}

void collectRequiredVariables(const SHInstanceData &data, ExposedInfo &out, const SHVar &var) {
  using namespace std::literals;

  switch (var.valueType) {
  case SHType::ContextVar: {
    auto sv = SHSTRVIEW(var);
    auto &ctx = compose::CompositionContext::get(data);
    auto info = ctx.findVariable(sv);
    if (info) {
      out.push_back(info->exposed);
      break;
    }
  } break;
  case SHType::Seq:
    shards::ForEach(var.payload.seqValue, [&](const SHVar &v) { collectRequiredVariables(data, out, v); });
    break;
  case SHType::Table:
    shards::ForEach(var.payload.tableValue, [&](const SHVar &key, const SHVar &v) { collectRequiredVariables(data, out, v); });
    break;
  default:
    break;
  }
}

namespace compose {
CompositionContext::CompositionContext() : visitedWires(tempAllocator.getAllocator()), stack(tempAllocator.getAllocator()) {}
CompositionContext::~CompositionContext() {
  while (stack.size() > 0) {
    popScope();
  }
  for (auto s : scopePool) {
    std::destroy_at(s);
  }
}

Scope::Scope(std::allocator_arg_t, allocator_type a) {}
Scope::Scope(std::allocator_arg_t, allocator_type a, Scope &&other) {}

void CompositionContext::step() {
  Scope *scope = &currentScope();
  ZoneScopedN("validateConnection");
  ZoneName(scope->bottom->name(scope->bottom), scope->bottom->nameLength);

  auto previousOutput = scope->previousOutputType;

  auto inputInfos = scope->bottom->inputTypes(scope->bottom);
  auto inputMatches = false;
  if (inputInfos.len == 1 && inputInfos.elements[0].basicType == SHType::None) {
    inputMatches = true;
  } else {
    for (uint32_t i = 0; inputInfos.len > i; i++) {
      auto &inputInfo = inputInfos.elements[i];
      if (matchTypes(previousOutput, inputInfo, true, true, true)) {
        inputMatches = true;
        break;
      }
    }
  }

  if (!inputMatches) {
    const auto msg = fmt::format(
        "Could not find a matching input type, shard: {} (line: {}, column: {}) expected: {}. Found instead: {}",
        scope->bottom->name(scope->bottom), scope->bottom->line, scope->bottom->column, inputInfos, scope->previousOutputType);
#if SH_DEBUG_TYPE_MATCHING
    for (uint32_t i = 0; inputInfos.len > i; i++) {
      auto &inputInfo = inputInfos.elements[i];
      if (matchTypes(previousOutput, inputInfo, true, true, true)) {
        inputMatches = true;
        break;
      }
    }
#endif
    throw ComposeError(msg);
  }

  auto copyComposeShared = [&](SHInstanceData &data) ALWAYS_INLINE {
    data.shard = scope->bottom;
    data.wire = scope->wire->source;
    data.inputType = previousOutput;
    // data.requiredVariables = scope->fullRequired;
    data.privateContext = (SHPrivateContext *)this;
    if (scope->next) {
      data.outputTypes = scope->next->inputTypes(scope->next);
    }
    data.onWorkerThread = scope->onWorkerThread;
  };

  flowAnnotateNextShard(scope->bottom);

  if (scope->bottom->compose) {
    SHInstanceData data{};
    {
      pmr::vector<SHExposedTypeInfo> sharedStorage{getAllocator()};
      pmr::unordered_set<std::string_view> exposedSet{getAllocator()};
      sharedStorage.reserve(scope->estimatedNumVariables);

      copyComposeShared(data);

      for (size_t si = 0; si < stack.size(); si++) {
        auto &scope = stack[si];
        for (auto &[k, v] : scope->variableMap) {
          if (!exposedSet.contains(k)) {
            sharedStorage.push_back(variables[v].exposed);
            exposedSet.insert(k);
          }
        }
        if (scope->hasAllVariables)
          break;
      }

      data.shared.elements = sharedStorage.data();
      data.shared.len = sharedStorage.size();
    }

    auto composeResult = scope->bottom->compose(scope->bottom, &data);
    scope = &currentScope(); // Refresh scope reference
    if (composeResult.error.code != SH_ERROR_NONE) {
      std::string_view msg(composeResult.error.message.string, size_t(composeResult.error.message.len));
      SHLOG_ERROR("Error composing shard: {}, wire: {}, shard: {}", msg, scope->wireName(), shardContextStr());
      throw ComposeError(msg);
    }
    scope->previousOutputType = composeResult.result;
  } else if (scope->bottom->composeV2) {
    SHInstanceData data{};
    copyComposeShared(data);

    auto composeResult = scope->bottom->composeV2(scope->bottom, &data);
    scope = &currentScope(); // Refresh scope reference
    if (composeResult.error.code != SH_ERROR_NONE) {
      std::string_view msg(composeResult.error.message.string, size_t(composeResult.error.message.len));
      SHLOG_ERROR("Error composing shard: {}, wire: {}, shard: {}", msg, scope->wireName(), shardContextStr());
      throw ComposeError(msg);
    }
    scope->previousOutputType = composeResult.result;
  } else {
    auto outputTypes = scope->bottom->outputTypes(scope->bottom);
    if (outputTypes.len == 1) {
      if (outputTypes.elements[0].basicType != SHType::Any) {
        scope->previousOutputType = outputTypes.elements[0];
      } else {
        auto inputTypes = scope->bottom->inputTypes(scope->bottom);
        if (inputTypes.len == 1 && inputTypes.elements[0].basicType != SHType::Any) {
          scope->previousOutputType = outputTypes.elements[0];
        }
      }
    } else {
      SHLOG_ERROR("Shard {} needs to implement the compose method", scope->bottom->name(scope->bottom));
      throw ComposeError("Shard has multiple possible output types and is missing the compose method");
    }
  }

#ifndef NDEBUG
  if (!scope->bottom->compose) {
    auto outputTypes = scope->bottom->outputTypes(scope->bottom);
    shards::IterableTypesInfo otypes(outputTypes);
    auto flowStopper = [&]() {
      if (strcmp(scope->bottom->name(scope->bottom), "Restart") == 0 || strcmp(scope->bottom->name(scope->bottom), "Stop") == 0 ||
          strcmp(scope->bottom->name(scope->bottom), "Return") == 0 || strcmp(scope->bottom->name(scope->bottom), "Fail") == 0) {
        return true;
      } else {
        return false;
      }
    }();

    auto shardHasValidOutputTypes =
        flowStopper || std::any_of(otypes.begin(), otypes.end(), [&](const auto &t) {
          return t.basicType == SHType::Any ||
                 (t.basicType == SHType::Seq && t.seqTypes.len == 1 && t.seqTypes.elements[0].basicType == SHType::Any &&
                  scope->previousOutputType.basicType == SHType::Seq) ||
                 (t.basicType == SHType::Table && scope->previousOutputType.basicType == SHType::Table) ||
                 t == scope->previousOutputType;
        });
    if (!shardHasValidOutputTypes) {
      auto msg = fmt::format("Shard {} doesn't have a valid output type", shardContextStr());
      throw ComposeError(msg);
    }
  }
#endif

  auto exposedVars = scope->bottom->exposedVariables(scope->bottom);
  for (uint32_t i = 0; exposedVars.len > i; i++) {
    auto &exposed_param = exposedVars.elements[i];
    std::string_view name(exposed_param.name);
    if (exposed_param.declared && scope->wire) {
      if (exposed_param.internalId != 0) {
        auto id = exposed_param.internalId & InternalIdValueMask;
        if (id >= variables.size()) {
          throw ComposeError(fmt::format("Invalid exposed variable id: {}, name: {}, at wire: {}, shard: {}", id, name,
                                         scope->wireName(), shardContextStr()));
        }
        auto &v = variables[id];
        if (v.exposed.exposedType != exposed_param.exposedType) {
          throw ComposeError(fmt::format("Variable {} declared twice with different types in wire {}, shard: {}", name,
                                         scope->wireName(), shardContextStr()));
        }

        auto existing = findVariable(name);
        if (!existing)
          scope->estimatedNumVariables++;
        scope->variableMap[name] = v.id;
        SPDLOG_LOGGER_DEBUG(logger, "Forward exposed variable: {} (id: {}) type: {} in {}", name, v.id, exposed_param.exposedType,
                            shardContextStr());
      } else {
        auto &v = insertVariable(name, exposed_param);
        SPDLOG_LOGGER_DEBUG(logger, "Declared variable: {} (id: {}) mutable: {}, type: {} in {}", name, v.id,
                            exposed_param.isMutable, exposed_param.exposedType, shardContextStr());
      }
    }
  }

  auto requiredVar = scope->bottom->requiredVariables(scope->bottom);

  // Deduplicate step
  pmr::unordered_map<pmr::string, SHExposedTypeInfo> requiredVars{getAllocator()};
  for (uint32_t i = 0; requiredVar.len > i; i++) {
    auto &required_param = requiredVar.elements[i];
    // pmr::string name(required_param.name);
    requiredVars.emplace(required_param.name, required_param);
  }

  for (const auto &required : requiredVars) {
    const auto &required_param = required.second;

    std::string_view name(required_param.name);

    auto foundInherited = findVariable(name);
    std::optional<SHExposedTypeInfo> found;
    if (foundInherited) {
      found = foundInherited->exposed;
    } else {
      found = std::nullopt;
    }
    if (!found) {
      auto err = fmt::format("Required variable not found: {}", name);
      SPDLOG_LOGGER_ERROR(logger, "Required variable not found: {}", name);
      throw ComposeError(err);
    } else {
      // Add tracking information
      currentShardInfo().variableRefs.push_back(foundInherited->id);

      auto exposedType = found->exposedType;
      auto requiredType = required_param.exposedType;
      if (!matchTypes(exposedType, requiredType, false, true, false)) {
        auto err = fmt::format("Required types do not match currently exposed ones for variable '{}' required type: (\"{}\", {})",
                               required.first, required.second.name, required.second.exposedType);
        SPDLOG_LOGGER_ERROR(logger,
                            "Required types do not match currently exposed ones for variable '{}' required type: (\"{}\", {})",
                            required.first, required.second.name, required.second.exposedType);
        throw ComposeError(err);
      }
    }
  }
}

Scope &CompositionContext::currentScope(size_t scopeOffset) {
  shassert(scopeOffset < stack.size() && "Scope index out of range");
  return *stack[stack.size() - 1 - scopeOffset];
}
const Scope &CompositionContext::currentScope(size_t scopeOffset) const {
  shassert(scopeOffset < stack.size() && "Scope index out of range");
  return *stack[stack.size() - 1 - scopeOffset];
}

Shard *CompositionContext::currentShard(size_t scopeOffset) const {
  auto &scope = currentScope(scopeOffset);
  return scope.bottom;
}

std::string_view CompositionContext::currentShardName() const {
  auto blk = currentScope().bottom;
  if (blk) {
    return std::string_view(blk->name(blk), blk->nameLength);
  }
  return "<none>";
}

std::string_view CompositionContext::shardContextStr(size_t scopeOffset) const {
  return shardContextStr(currentShard(scopeOffset));
}

std::string_view CompositionContext::shardContextStr(Shard *shard) const {
  shardContextStrBuf.clear();
  if (shard) {
    fmt::format_to(std::back_inserter(shardContextStrBuf), "{} (line: {}, column: {})", shard->name(shard), shard->line,
                   shard->column);
  } else {
    fmt::format_to(std::back_inserter(shardContextStrBuf), "<none>");
  }
  return shardContextStrBuf;
}

VariableRef CompositionContext::findVariable(std::string_view name, size_t scopeOffset) {
  size_t ss = stack.size();
  for (size_t i0 = scopeOffset; i0 < ss; i0++) {
    size_t idx0 = ss - i0 - 1;
    auto &scope = stack[idx0];
    shassert(scope->wire && "Wire should be valid");
    auto it = scope->variableMap.find(name);
    if (it != scope->variableMap.end()) {
      auto varPtr = &variables[it->second];
      auto wire = scope->wire.get();
      return VariableRef{varPtr, wire};
    }
    if (scope->hasAllVariables)
      break;
  }
  return VariableRef{};
}

VariableRef CompositionContext::findVariable(uint32_t id_, size_t scopeOffset) {
  auto &scope = *stack[stack.size() - 1 - scopeOffset];
  auto &wire = *scope.wire;
  shassert((id_ & InternalIdFlagsInternal) == InternalIdFlagsInternal && "Invalid internal variable ID");
  auto id = id_ & InternalIdValueMask;
  if (id >= variables.size()) {
    throw std::logic_error(fmt::format("Invalid variable id: {}", id));
  }
  return VariableRef{&variables[id], &wire};
}

Variable &CompositionContext::insertVariable(std::string_view name, SHExposedTypeInfo type) {
  auto &scope = currentScope();

  bool wasExisting = findVariable(name);
  if (!wasExisting)
    scope.estimatedNumVariables++;

  auto &newVar = insertAnonymousVariable(type);
  scope.variableMap[name] = newVar.id;
  return newVar;
}

Variable &CompositionContext::insertAnonymousVariable(SHExposedTypeInfo type) {
  auto &scope = currentScope();
  shassert(scope.wire && "Wire should be valid");
  size_t newId = variables.size();
  auto &newVar = variables.emplace_back(Variable{newId, scope.id, VariableKind::Local, type});
  newVar.exposed.internalId = newId | InternalIdFlagsInternal;
  currentShardInfo().variableRefs.push_back(newId);
  return newVar;
}

ComposedWire::ShardInfo &CompositionContext::currentShardInfo() {
  auto &scope = currentScope();
  shassert(scope.wire && "Wire should be valid");
  auto currentShard = this->currentShard();
  if (currentShard) {
    shassert(currentShard->id != 0);
    auto it = scope.wire->shardSeqId.find(currentShard->id);
    if (it == scope.wire->shardSeqId.end()) {
      it = scope.wire->shardSeqId.emplace(currentShard->id, ComposedWire::ShardInfo{}).first;
      it->second.shard = currentShard;
      currentShard->seqId = it->second.seqId = scope.wire->shardSeqId.size();
    }
    return it->second;
  } else {
    auto &shardInfo = scope.wire->shardSeqId[0];
    shardInfo.seqId = 0;
    return shardInfo;
  }
}

compose::Scope &CompositionContext::pushScope(std::optional<SHTypeInfo> inputType) {
  compose::Scope *e{};
  if (scopePool.size() > 0) {
    e = scopePool.back(), scopePool.pop_back();
    e->reset();
  } else {
    e = getAllocator().new_object<Scope>();
  }
  e->id = idAllocator++;

  if (inputType)
    SPDLOG_LOGGER_DEBUG(logger, "Push scope ({}) < {}", e->id, *inputType);
  else
    SPDLOG_LOGGER_DEBUG(logger, "Push scope ({})", e->id);
  auto &s = *stack.emplace_back(e);
  if (inputType) {
    s.previousOutputType = s.originalInputType = *inputType;
    s.flow.inputType = *inputType;
    s.flow.currentVariableType = s.flow.inputType;
    s.flow.currentAccess.emplace(std::allocator_arg, getAllocator(), flow::VariableAccessor::input());
  }
  return s;
}
void CompositionContext::popScope() {
  auto scope = stack.back();
  // Commit rule
  auto comitted = flowCommitRule();
  if (comitted) {
    SPDLOG_LOGGER_DEBUG(logger, "Pop scope({}): Committed rule: {}", scope->id, *comitted);
  }

  // Analyze contents
  auto &rules = current().commitedRules;
  if (rules.matches.size() > 0) {
    SPDLOG_LOGGER_DEBUG(logger, "Assertion rules: {}", rules);
  }

  stack.pop_back();
  scopePool.push_back(scope);
  SPDLOG_LOGGER_DEBUG(logger, "Popped scope ({})", scope->id);
}
void CompositionContext::annotateContextVariable(std::string_view name, std::optional<SHTypeInfo> type) {
  flowAnnotateContextVariable(name, type);
  flowTagInstrumented();
}
void CompositionContext::annotateSubPath(const SHVar &key) {
  flowAppendVA(flow::VariableAccessor::key(key));
  SPDLOG_LOGGER_DEBUG(logger, "Annotating sub-path: {}", key);
  flowTagInstrumented();
}
void CompositionContext::annotateClearVariable() {
  flowTagInstrumented();
  flowClearVariable();
}
void CompositionContext::annotatePassthrough() {
  flowTagInstrumented();
  flowTagPassthrough();
}
void CompositionContext::annotateIsOfType(SHTypeInfo type) {
  flowTagInstrumented();
  auto r = flowAnnotateIsA(type);
  if (r)
    SPDLOG_LOGGER_DEBUG(logger, "Rule: {}", *r);
}
void CompositionContext::annotateNot() {
  flowTagInstrumented();
  auto &r = current().stackRule;
  if (r) {
    if (!current().currentVariableType) {
      throw std::logic_error("Undefined previous variable to negation");
    }
    if (current().currentVariableType != shards::CoreInfo::BoolType) {
      throw std::logic_error(fmt::format("Cannot negate non-boolean variable ({})", *current().currentVariableType));
    }
    r = r->inverted();
    SPDLOG_LOGGER_DEBUG(logger, "Not: Inverted rule: {}", *r);
  } else {
    SPDLOG_LOGGER_DEBUG(logger, "Not: No rule to invert");
  }
}
void CompositionContext::annotateAnd() {
  flowTagInstrumented();
  // Commit current rule since it will always be true
  auto comitted = flowCommitRule();
  if (comitted) {
    SPDLOG_LOGGER_DEBUG(logger, "And: Committed rule: {}", *comitted);
  } else {
    SPDLOG_LOGGER_DEBUG(logger, "And: No rule to commit");
  }

  // Rebase flow
  auto &c = current();
  c.currentVariableType = c.inputType;
  c.currentAccess.emplace(std::allocator_arg, getAllocator(), flow::VariableAccessor::input());
}
void CompositionContext::annotateOr() {
  flowTagInstrumented();
  flowClearUndeterministic();

  // Rebase flow
  auto &c = current();
  c.currentVariableType = c.inputType;
  c.currentAccess.emplace(std::allocator_arg, getAllocator(), flow::VariableAccessor::input());
}
void CompositionContext::annotateIsNone() {
  flowTagInstrumented();
  auto r = flowAnnotateIsA(shards::CoreInfo::NoneType);
  if (r)
    SPDLOG_LOGGER_DEBUG(logger, "Rule: {}", *r);
}
void CompositionContext::annotateIsNotNone() {
  flowTagInstrumented();
  if (flowAnnotateIsA(shards::CoreInfo::NoneType)) {
    auto &r = current().stackRule;
    r = r->inverted();
    SPDLOG_LOGGER_DEBUG(logger, "Rule: {}", *r);
  }
}

void CompositionContext::flowAnnotateNextShard(Shard *shard) {
  auto &c = current();
  if (c.shardIndex > 0 && !c.annotations.isInstrumented) {
    // SPDLOG_LOGGER_DEBUG(logger, "non-instrumented shard: {} ({})", c.shardName, c.shardIndex);
    flowClearUndeterministic();
  }
  c.shardName = shard->name(shard);
  c.annotations.reset();

  // Force creation of shard info/seqid
  currentShardInfo();
  SPDLOG_LOGGER_DEBUG(logger, "Annotating next shard: {} ({})", c.shardName, c.shardIndex);
}

void CompositionContext::flowAnnotateContextVariable(std::string_view name, std::optional<SHTypeInfo> type) {
  if (type) {
    SPDLOG_LOGGER_DEBUG(logger, "Annotating context variable: {} with type: {}", name, *type);
  } else {
    SPDLOG_LOGGER_DEBUG(logger, "Annotating context variable: {}", name);
  }
  auto &s = current();
  s.currentVariableType = type;
  s.currentAccess.emplace(std::allocator_arg, getAllocator(), flow::VariableAccessor::var(name));
}

void CompositionContext::flowClearVariable() {
  auto &v = current().currentAccess;
  if (v) {
    SPDLOG_LOGGER_DEBUG(logger, "Clearing variable: {}", *v);
  }
  v.reset();
}

void CompositionContext::flowAppendVA(flow::VariableAccessor va) {
  auto &c = current();
  if (!c.currentAccess) {
    SPDLOG_LOGGER_ERROR(logger, "Undefined variable access({}) (input missing)", va);
    flowClearUndeterministic();
    return;
  }
  c.currentAccess->append(va);
}

void CompositionContext::flowClearUndeterministic() {
  auto &c = current();
  c.currentAccess.reset();
  c.currentVariableType = std::nullopt;
  c.stackRule.reset();
}

AssertionRule *CompositionContext::flowAnnotateIsA(SHTypeInfo type) {
  auto &c = current();
  if (!c.currentAccess) {
    SPDLOG_LOGGER_ERROR(logger, "Undefined variable access of IsXXX");
    flowClearUndeterministic();
    return nullptr;
  }

  std::optional<flow::VariableAccessorChain> va;
  c.currentAccess.swap(va);
  c.stackRule = AssertionRule_IsA{*va, TypeInfo(type)};
  c.currentVariableType = shards::CoreInfo::BoolType;
  return &*c.stackRule;
}

AssertionRule *CompositionContext::flowCommitRule() {
  auto &c = current();
  if (c.stackRule) {
    auto p = &c.commitedRules.matches.emplace_back(std::move(*c.stackRule));
    c.stackRule.reset();
    return p;
  }
  return nullptr;
}

std::string_view Scope::wireName() const { return wire ? std::string_view(wire->source->name) : std::string_view("(unwired)"); }

ComposedWire::ComposedWire(SHWire *source) : source(source) {}

SHComposeResult internalComposeWire(const std::vector<Shard *> &wire, SHInstanceData data, bool fromWire) {
  ZoneScoped;
  if (data.wire) {
    ZoneText(data.wire->name.data(), data.wire->name.size());
  }

  // Optionally create a context
  std::optional<CompositionContext> ownedContext{};
  if (!data.privateContext) {
    ZoneScopedN("new CompositionContext");
    ownedContext.emplace();
    data.privateContext = (SHPrivateContext *)&ownedContext.value();
  }

  CompositionContext &ctx = CompositionContext::get(data);

  auto &scope = ctx.pushScope(data.inputType);
  DEFER({ ctx.popScope(); });
  scope.originalInputType = data.inputType;
  scope.previousOutputType = data.inputType;
  scope.onWorkerThread = data.onWorkerThread;

  // Find the first specification of a given wire and treat it as the root of that wire
  auto it = ctx.wires.find(data.wire);
  std::shared_ptr<compose::ComposedWire> composeWireRoot;
  if (it == ctx.wires.end()) {
    it = ctx.wires.emplace(data.wire, std::make_shared<compose::ComposedWire>(data.wire)).first;
    composeWireRoot = it->second;
    SPDLOG_LOGGER_DEBUG(compose::logger, "Composing new wire: {}", data.wire->name);
  } else {
    auto parentShard = ctx.currentShard(1);
    scope.bottom = parentShard;
  }
  scope.wire = it->second;

  std::shared_ptr<WireRuntimeVariableInfo> runtimeVariableInfo;
  try {
    if (composeWireRoot) {
      auto &sourceWire = data.wire;
      if (sourceWire->runtimeVariableInfo)
        throw std::logic_error(fmt::format("Wire {} is already composed/being composed", sourceWire->name));
      runtimeVariableInfo = sourceWire->runtimeVariableInfo = std::make_shared<WireRuntimeVariableInfo>();

      for (const auto &[key, pVar] : scope.wire->source->getExternalVariables()) {
        const SHExternalVariable &extVar = pVar;
        const SHVar &var = *extVar.var;
        if ((var.flags & SHVAR_FLAGS_EXTERNAL) == 0) {
          throw std::runtime_error(fmt::format("Variable '{}' must have SHVAR_FLAGS_EXTERNAL flag set", key.payload.stringValue));
        }

        const SHTypeInfo *type{};
        if (extVar.type) {
          type = extVar.type;
        } else {
          static TypeCache typeCache;
          type = &typeCache.insertUnique(TypeInfo(var, data, nullptr, true, true));
        }

        auto &v = ctx.insertVariable(key.payload.stringValue,
                                     SHExposedTypeInfo{key.payload.stringValue, {}, *type, true /* mutable */});
        v.kind = VariableKind::External;
        v.exposed.tracked = var.flags & SHVAR_FLAGS_TRACKED;
      }

      // add present mesh variables as well if we have a mesh
      auto mesh = scope.wire->source->mesh.lock();
      if (mesh) {
        for (auto &v : mesh->getVariables()) {
          // only add variables with metadata basically
          auto metadata = mesh->getMetadata(&v.second);
          if (metadata) {
            std::string_view sName(v.first.payload.stringValue, v.first.payload.stringLen);
            auto &v = ctx.insertVariable(sName, *metadata);
            v.kind = VariableKind::Global;
          }
        }
      }
    }

    // Fully specify all variables
    scope.hasAllVariables = true;

    if (data.shared.elements) {
      auto insertExistingVariable = [&](VariableRef v1) {
        if (composeWireRoot) {
          // Create an external variable reference
          auto &v = ctx.insertVariable(v1->exposed.name, v1->exposed);
          SPDLOG_LOGGER_DEBUG(compose::logger, "Forwarding variable: {} (id: {}) to wire {}", v.exposed.name, v.id,
                              scope.wireName());
          v.kind = VariableKind::Inherited;
        } else {
          // Just forward the same instance
          scope.variableMap[v1->exposed.name] = v1->id;
        }
      };
      for (uint32_t i = 0; i < data.shared.len; i++) {
        auto &info = data.shared.elements[i];
        bool done = false;
        VariableRef variable{};
        if (info.internalId != 0) {
          variable = ctx.findVariable(info.internalId, 1);
          if (!variable)
            throw std::logic_error(
                fmt::format("Variable '{}' (as id: {}) not found in wire '{}'", info.name, info.internalId, scope.wireName()));
          insertExistingVariable(variable);
          done = true;
        }

        if (!done && info.internalId == 0) {
          variable = ctx.findVariable(info.name, 1);
          if (variable && variable->exposed.exposedType == info.exposedType) {
            insertExistingVariable(variable);
            done = true;
          }
        }

        if (!done) {
          SPDLOG_LOGGER_TRACE(compose::logger, "Internally scoped variable '{}' declared in shard {}",
                              data.shared.elements[i].name, ctx.shardContextStr(1));
          ctx.insertVariable(info.name, info);
        }
      }
    }

    size_t chsize = wire.size();
    for (size_t i = 0; i < chsize; i++) {
      auto &scope = ctx.currentScope();
      Shard *blk = wire[i];
      scope.next = nullptr;
      if (i < chsize - 1)
        scope.next = wire[i + 1];

      scope.bottom = blk;
      try {
        ctx.step();
      } catch (std::exception &ex) {
        auto verboseMsg = fmt::format("Error composing shard: {}, line: {}, column: {}, wire: {}, error: {}", blk->name(blk),
                                      blk->line, blk->column, scope.wireName(), ex.what());
        SHLOG_ERROR("{}", verboseMsg);
        if (data.wire) {
          auto mesh = data.wire->mesh.lock();
          if (mesh) {
            std::string_view what{ex.what()};
            shards::OwnedVar err{Var(what)};
            mesh->dispatcher.trigger<SHWire::OnErrorEvent>({data.wire, blk, std::move(err)});
          }
        }
        throw ComposeError(verboseMsg);
      }
    }

    SHComposeResult result = {scope.previousOutputType};

    // Check referenced variables, not declared locally
    pmr::unordered_set<size_t> usedVariables(ctx.getAllocator());
    for (auto &v : scope.usedVariables) {
      auto &var = ctx.variables[v];
      if (var.declaredIn == scope.id && var.kind == VariableKind::Local) {
        continue; // Ignore locally declared variables
      }

      if (!usedVariables.contains(v)) {
        usedVariables.insert(v);
        auto &var = ctx.variables[v];
        shards::arrayPush(result.requiredInfo, var.exposed);
      }
    }

    // Insert wire root scope variables into exposed
    for (auto &[k, v] : scope.variableMap) {
      auto &var = ctx.variables[v];
      if (var.kind == VariableKind::Local && var.declaredIn == scope.id) {
        shards::arrayPush(result.exposedInfo, var.exposed);
      }
    }

    if (wire.size() > 0) {
      auto &last = wire.back();
      if (strcmp(last->name(last), "Restart") == 0 || strcmp(last->name(last), "Return") == 0 ||
          strcmp(last->name(last), "Fail") == 0) {
        result.flowStopper = true;
      } else if (strcmp(last->name(last), "Stop") == 0) {
        // need to check if first param is none
        auto fp = last->getParam(last, 0);
        if (fp.valueType == SHType::None)
          result.flowStopper = true;
      }
    }

    if (composeWireRoot) {
      // Finalize and populate wire variable data
      SPDLOG_LOGGER_DEBUG(compose::logger, "Wire {} analysis", scope.wireName());
      pmr::set<size_t> allRequiredVariables(ctx.getAllocator());
      pmr::map<size_t, compose::ComposedWire::ShardInfo *> orderedShards(ctx.getAllocator());
      pmr::vector<WireRuntimeVariableInfo::VariableDecl> externalVariables(ctx.getAllocator());
      pmr::vector<WireRuntimeVariableInfo::VariableDecl> inheritedVariables(ctx.getAllocator());
      pmr::unordered_map<size_t, size_t> variableRemapping(ctx.getAllocator());
      using VarDecl = WireRuntimeVariableInfo::VariableDecl;
      for (auto &[k, v] : composeWireRoot->shardSeqId) {
        orderedShards.emplace(v.seqId, &v);
      }

      // First pass: collect required & references wire variables
      for (auto &[k, shardInfo] : orderedShards) {
        // Ignore wire root (seqid == 0) since it's only used to populate inherited variables, and we only want to collect usages
        if (k == 0) {
          bool okay = true;
          for (auto &v : shardInfo->variableRefs) {
            auto &var = ctx.variables[v];
            if (var.kind == VariableKind::Local) {
              okay = false;
              SPDLOG_LOGGER_ERROR(compose::logger, "Wire root must not have local variables, found {} (id: {}, type: {})",
                                  var.exposed.name, var.id, var.exposed.exposedType);
            }
          }
          if (!okay) {
            throw std::logic_error("Wire root must not have local variables");
          }
          continue;
        }

        auto shard = shardInfo->shard;
        SPDLOG_LOGGER_DEBUG(compose::logger, " [{}] Shard {}, Id: {}, SId: {}", k, ctx.shardContextStr(shard),
                            shard ? shard->id : 0, shardInfo->seqId);
        pmr::set<size_t> logUniqueRefs(ctx.getAllocator());
        for (auto &v : shardInfo->variableRefs) {
          auto &var = ctx.variables[v];
          if (!logUniqueRefs.contains(v)) {
            SPDLOG_LOGGER_DEBUG(compose::logger, " - Variable: {} (id: {}, kind: {}, type: {})", var.exposed.name, var.id,
                                magic_enum::enum_name(var.kind), var.exposed.exposedType);
            logUniqueRefs.insert(v);
          }

          auto targetIt = variableRemapping.find(v);
          if (targetIt == variableRemapping.end()) {
            VarDecl vd{
                .name = var.exposed.name,
                .type = var.exposed.exposedType,
            };
            auto &vs = runtimeVariableInfo->variableScopes.emplace(shardInfo->seqId, WireRuntimeVariableInfo::VariableScope{})
                           .first->second;
            if (var.kind == VariableKind::Inherited) {
              size_t newVarId = inheritedVariables.size() | WireRuntimeVariableInfo::IdFlagsInherited;
              vs.variableLookup[var.exposed.name] = newVarId;
              inheritedVariables.emplace_back(std::move(vd));
              targetIt = variableRemapping.emplace(v, newVarId).first;

              allRequiredVariables.insert(v);
            } else if (var.kind == VariableKind::External) {
              size_t newVarId = externalVariables.size() | WireRuntimeVariableInfo::IdFlagsExternal;
              vs.variableLookup[var.exposed.name] = newVarId;
              externalVariables.emplace_back(std::move(vd));
              targetIt = variableRemapping.emplace(v, newVarId).first;
            } else if (var.kind == VariableKind::Global) {
              size_t newVarId = runtimeVariableInfo->globalVariables.size() | WireRuntimeVariableInfo::IdFlagsGlobal;
              vs.variableLookup[var.exposed.name] = newVarId;
              runtimeVariableInfo->globalVariables.emplace_back(std::move(vd));
              targetIt = variableRemapping.emplace(v, newVarId).first;
            } else if (var.kind == VariableKind::Local) {
              size_t newVarId = runtimeVariableInfo->localVariables.size();
              vs.variableLookup[var.exposed.name] = newVarId;
              runtimeVariableInfo->localVariables.emplace_back(std::move(vd));
              targetIt = variableRemapping.emplace(v, newVarId).first;
            }
          }

          auto &vs = runtimeVariableInfo->variableScopes.emplace(shardInfo->seqId, WireRuntimeVariableInfo::VariableScope{})
                         .first->second;
          vs.variableLookup[var.exposed.name] = targetIt->second;
        }

        // Populate wire->requirements
        if (data.wire) {
          auto &outReqs = data.wire->requirements;
          outReqs.clear();
          for (auto &req : allRequiredVariables) {
            auto &v = ctx.variables[req];
            outReqs.emplace(v.exposed.name, v.exposed);
          }
        }
      }

      runtimeVariableInfo->numExternalVariables = externalVariables.size();
      for (auto &v : externalVariables) {
        runtimeVariableInfo->externalAndInheritedVariables.emplace_back(std::move(v));
      }
      for (auto &v : inheritedVariables) {
        runtimeVariableInfo->externalAndInheritedVariables.emplace_back(std::move(v));
      }

      for (auto &v : allRequiredVariables) {
        auto &var = ctx.variables[v];
        SPDLOG_LOGGER_DEBUG(compose::logger, " Wire required variable: {} (id: {}, type: {})", var.exposed.name, var.id,
                            var.exposed.exposedType);
      }

      SPDLOG_LOGGER_DEBUG(compose::logger, "Wire {} analysis done", scope.wireName());
      runtimeVariableInfo->initStorage();
    }

    return result;
  } catch (...) {
    if (runtimeVariableInfo) {
      data.wire->runtimeVariableInfo.reset();
    }
    throw;
  }
}

} // namespace compose

SHComposeResult internalComposeWire(const std::vector<Shard *> &wire, SHInstanceData data, bool fromWire = false) {
  return compose::internalComposeWire(wire, data, fromWire);
}

SHComposeResult composeWire(const std::vector<Shard *> &wire, SHInstanceData data) {
  // We need to catch exceptions here and add them to the context
  try {
    return internalComposeWire(wire, data);
  } catch (std::exception &ex) {
    if (data.privateContext) {
      CompositionContext *context = reinterpret_cast<CompositionContext *>(data.privateContext);
      context->errorStack.push_back(ex.what());
    }
    throw;
  }
}

void validateWireTraits(const SHWire *wire, const SHComposeResult &cr) {
  TraitMatcher tm;
  for (auto &trait : wire->getTraits()) {
    if (!tm(cr.exposedInfo, trait)) {
      throw ComposeError(fmt::format("Wire {} does not implement {}:\n{}", wire->name, trait, tm.error));
    }
  }
}

SHComposeResult internalComposeWire(const SHWire *wire_, SHInstanceData data) {
  SHWire *wire = const_cast<SHWire *>(wire_);

  // compare exchange and then shassert we were not composing
  bool expected = false;
  if (!wire->composing.compare_exchange_strong(expected, true)) {
    SHLOG_ERROR("Wire {} is already being composed", wire->name);
    throw ComposeError("Wire is already being composed");
  }
  // defer reset compose state
  DEFER(wire->composing.store(false));

  // settle input type of wire before compose
  if (wire->shards.size() > 0 && strncmp(wire->shards[0]->name(wire->shards[0]), "Expect", 6) == 0) {
    // If first shard is an Expect, this wire can accept ANY input type as the type is checked at runtime
    wire->inputType = SHTypeInfo{SHType::Any};
    wire->ignoreInputTypeCheck = true;
  } else if (wire->shards.size() > 0 && !std::any_of(wire->shards.begin(), wire->shards.end(), [&](const auto &shard) {
               return strcmp(shard->name(shard), "Input") == 0;
             })) {
    // If first shard is a plain None, mark this wire has None input
    // But make sure we have no (Input) shards
    auto inTypes = wire->shards[0]->inputTypes(wire->shards[0]);
    if (inTypes.len == 1 && inTypes.elements[0].basicType == SHType::None) {
      wire->inputType = SHTypeInfo{};
      wire->ignoreInputTypeCheck = true;
    } else {
      wire->inputType = data.inputType;
      wire->ignoreInputTypeCheck = false;
    }
  } else {
    wire->inputType = data.inputType;
    wire->ignoreInputTypeCheck = false;
  }

  shassert(wire == data.wire); // caller must pass the same wire as data.wire

  auto res = internalComposeWire(wire->shards, data, true);
  DEFER({
    shards::arrayFree(res.exposedInfo);
    shards::arrayFree(res.requiredInfo);
  });

  validateWireTraits(wire, res);

  // set output type
  wire->outputType = res.outputType;

  // validate wire output types for additional return paths
  if (wire->composeData) {
    auto &cd = *wire->composeData.get();
    DEFER({ wire->composeData.reset(); });
    for (auto &type : cd.outputTypes) {
      if (!matchTypes(type, res.outputType, true, true, true)) {
        std::string err =
            fmt::format("Possible output {} does not match main output type: {} for wire {}", type, res.outputType, wire->name);
        throw ComposeError(err);
      }
    }
  }

  SHComposeResult result{};
  // swap to avoid deferred free
  std::swap(result, res);
  return result;
}

SHComposeResult composeWire(const SHWire *wire_, SHInstanceData data) {
  // We need to catch exceptions here and add them to the context
  try {
    return internalComposeWire(wire_, data);
  } catch (std::exception &ex) {
    if (data.privateContext) {
      CompositionContext *context = reinterpret_cast<CompositionContext *>(data.privateContext);
      context->errorStack.push_back(ex.what());

      // also send error event if possible
      auto mesh = wire_->mesh.lock();
      if (mesh) {
        std::string_view what{ex.what()};
        shards::OwnedVar err{Var(what)};
        mesh->dispatcher.trigger<SHWire::OnErrorEvent>({wire_, nullptr, std::move(err)});
      }
    }
    throw;
  }
}

SHComposeResult composeWire(const Shards wire, SHInstanceData data) {
  std::vector<Shard *> shards;
  for (uint32_t i = 0; wire.len > i; i++) {
    shards.push_back(wire.elements[i]);
  }
  return composeWire(shards, data);
}

SHComposeResult composeWire(const SHSeq wire, SHInstanceData data) {
  std::vector<Shard *> shards;
  for (uint32_t i = 0; wire.len > i; i++) {
    shards.push_back(wire.elements[i].payload.shardValue);
  }
  return composeWire(shards, data);
}

bool validateSetParam(Shard *shard, int index, const SHVar &value) {
  auto params = shard->parameters(shard);
  if (params.len <= (uint32_t)index) {
    SHLOG_ERROR("Parameter index out of range, shard: {}, line: {}, column: {}", shard->name(shard), shard->line, shard->column);
    return false;
  }

  auto param = params.elements[index];

  // Build a SHTypeInfo for the var
  SHInstanceData data{};
  auto varType = deriveTypeInfo(value, data);
  DEFER(freeTypeInfo(varType));

  for (uint32_t i = 0; param.valueTypes.len > i; i++) {
    // This only does a quick check to see if the type is roughly correct
    // ContextVariable types will be checked in validateConnection based on requiredVariables
    if (matchTypes(varType, param.valueTypes.elements[i], true, true, true)) {
      return true; // we are good just exit
    }
  }

  auto err = fmt::format("Parameter {} not accepting this kind of variable: {} (type: {}, valid types: {}), line: {}, column: {}",
                         param.name, value, varType, param.valueTypes, shard->line, shard->column);
#if SH_DEBUG_TYPE_MATCHING
  // Put a breakpoint here to debug
  for (uint32_t i = 0; param.valueTypes.len > i; i++) {
    if (matchTypes(varType, param.valueTypes.elements[i], true, true, true)) {
      return true;
    }
  }
#endif
  SHLOG_ERROR("{}", err);
  return false;
}

} // namespace shards
void SHMesh::prettyCompose(const std::shared_ptr<SHWire> &wire, SHInstanceData &data) {
  shards::CompositionContext privateContext;
  data.privateContext = (SHPrivateContext *)&privateContext;
  try {
    auto validation = shards::composeWire(wire.get(), data);
    shards::arrayFree(validation.exposedInfo);
    shards::arrayFree(validation.requiredInfo);
  } catch (const std::exception &e) {
    // build a reverse stack error log from privateContext.errorStack
    std::string errors;
    for (auto it = privateContext.errorStack.rbegin(); it != privateContext.errorStack.rend(); ++it) {
      errors += *it;
      if (++it == privateContext.errorStack.rend())
        break;
      errors += "\n";
    }
    SHLOG_ERROR("Wire {} failed to compose:\n{}", wire->name, errors);
    throw;
  }
}

void SHMesh::compose(const std::shared_ptr<SHWire> &wire, SHVar input) {
  ZoneScoped;

  SHLOG_TRACE("Composing wire {}", wire->name);

  if (wire->warmedUp) {
    SHLOG_ERROR("Attempted to Pre-composing a wire multiple times, wire: {}", wire->name);
    throw shards::SHException("Multiple wire Pre-composing");
  }

  wire->mesh = shared_from_this();

  wire->isRoot = true;
  // remove when done here
  DEFER(wire->isRoot = false);

  // compose the wire
  SHInstanceData data = instanceData;
  data.wire = wire.get();
  data.inputType = shards::deriveTypeInfo(input, data);
  DEFER({ shards::freeDerivedInfo(data.inputType); });
  prettyCompose(wire, data);

  SHLOG_TRACE("Wire {} composed", wire->name);
}
