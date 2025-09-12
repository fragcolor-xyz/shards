#include "compose.hpp"
#include "pmr/unordered_set.hpp"
#include "pmr/vector.hpp"
#include "pmr/string.hpp"
#include "pmr/set.hpp"
#include "pmr/map.hpp"
#include "type_cache.hpp"
#include "compose_fmt.hpp"
#include "visit_helper.hpp"
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
CompositionContext::CompositionContext()
    : visitedWires(tempAllocator.getAllocator()), //
      scopePool(tempAllocator.getAllocator()),    //
      stack(tempAllocator.getAllocator()),        //
      wires(tempAllocator.getAllocator()), rootReferenceMap(tempAllocator.getAllocator()) {}
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

  const uint32_t composeInputTag = 0xff11ff11;
  auto copyComposeShared = [&](SHInstanceData &data) ALWAYS_INLINE {
    data.shard = scope->bottom;
    data.wire = scope->wire->source;
    data.inputType = previousOutput;
    data.inputType.tag = composeInputTag;
    data.privateContext = (SHPrivateContext *)this;
    if (scope->next) {
      data.outputTypes = scope->next->inputTypes(scope->next);
    }
    data.onWorkerThread = scope->onWorkerThread;
  };

  flowAnnotateNextShard(scope->bottom);

  bool hasCompose = false;
  if (scope->bottom->composeV2) { // Prefer over v1
    SHInstanceData data{};
    copyComposeShared(data);
    auto composeResult = scope->bottom->composeV2(scope->bottom, &data);
    if (composeResult.error.code != SH_ERROR_NONE) {
      std::string_view msg(composeResult.error.message.string, size_t(composeResult.error.message.len));
      SHLOG_ERROR("Error composing shard: {}, wire: {}, shard: {}", msg, scope->wireName(), shardContextStr());
      throw ComposeError(msg);
    }
    scope->previousOutputType = composeResult.result;
    hasCompose = true;
  } else if (scope->bottom->compose) {
    SHInstanceData data{};
    {
      pmr::vector<SHExposedTypeInfo> sharedStorage{getAllocator()};
      pmr::unordered_set<std::string_view> exposedSet{getAllocator()};
      sharedStorage.reserve(scope->estimatedNumVariables);

      copyComposeShared(data);

      for (size_t si = 0; si < stack.size(); si++) {
        auto &scope = stack[stack.size() - 1 - si];
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
    if (composeResult.error.code != SH_ERROR_NONE) {
      std::string_view msg(composeResult.error.message.string, size_t(composeResult.error.message.len));
      SHLOG_ERROR("Error composing shard: {}, wire: {}, shard: {}", msg, scope->wireName(), shardContextStr());
      throw ComposeError(msg);
    }
    scope->previousOutputType = composeResult.result;
    hasCompose = true;
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

  if (!scope->annotations.isAnnotated) {
    if (scope->previousOutputType.tag == composeInputTag) {
      annotatePassthrough();
      SPDLOG_LOGGER_DEBUG(logger, "Shards {} (id: {}) is passthrough by compose", scope->shardName, scope->bottom->id);
    }
  }

  // if (!hasCompose && !scope->annotations.isAnnotated) {
  // throw ComposeError(fmt::format("non-annotated shard: {} (id: {})", scope->shardName, scope->bottom->id));
  // }
  if (!hasCompose && !scope->annotations.isAnnotated) {
    SPDLOG_LOGGER_DEBUG(logger, "non-annotated shard: {} (id: {})", scope->shardName, scope->bottom->id);
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
    if (exposed_param.declared) {
      shassert(scope->wire && "Wire should be valid");
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

        auto &shard = currentShardInfo();
        // When this variable comes from a scope inside the current shard (scope id will be higher than current, assuming
        // composition happens within this shard's compose)
        bool isForwardedInternally = v.declaredIn > scope->id;
        if (isForwardedInternally) {
          SPDLOG_LOGGER_DEBUG(logger, "Forwarding exposed variable: {} (id: {}) type: {} in {}", name, v.id,
                              exposed_param.exposedType, shardContextStr());
        } else {
          if (v.exposed.exposedType != exposed_param.exposedType) {
            SPDLOG_LOGGER_DEBUG(logger, "Updating exposed variable: {} (id: {}) type: {} in {}", name, v.id,
                                exposed_param.exposedType, shardContextStr());
            SPDLOG_LOGGER_DEBUG(logger, "  variable: {} (id: {}), updated type: {}", name, v.id, exposed_param.exposedType);
            v.exposed.exposedType = exposed_param.exposedType;
          }
        }

        scope->variableMap[name] = v.id;

        shard.variableRefs.push_back(v.id);
      } else {
        auto v = insertVariable(name, exposed_param);
        SPDLOG_LOGGER_DEBUG(logger, "Declared variable: {} (id: {}) mutable: {}, type: {} in {}", name, v->id,
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

      // Validate reference
      if (foundInherited->referenceTarget) {
        checkReferenceIsValid(*foundInherited->referenceTarget, foundInherited->referenceVersion);
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

std::string_view CompositionContext::shardContextStr(const Shard *shard) const {
  shardContextStrBuf.clear();
  if (shard) {
    auto cs = const_cast<Shard *>(shard);
    fmt::format_to(std::back_inserter(shardContextStrBuf), "{} ({})", cs->name(cs), shards::formatShardSourceLocation(cs));
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
      return varPtr;
    }
    if (scope->hasAllVariables)
      break;
  }
  return VariableRef{};
}

VariableRef CompositionContext::findVariable(uint32_t id_, size_t scopeOffset) {
  uint32_t id = id_ & InternalIdValueMask;
  // shassert((id_ & InternalIdFlagsInternal) == InternalIdFlagsInternal && "Invalid internal variable ID");
  // auto &scope = *stack[stack.size() - 1 - scopeOffset];
  if (id >= variables.size()) {
    throw std::logic_error(fmt::format("Invalid variable id: {}", id));
  }
  return &variables[id];
}

const VersionedReference &CompositionContext::currentAccess() const {
  auto &scope = currentScope();
  if (!scope.currentAccess) {
    throw std::logic_error("No variable access set, can not assign to a reference");
  }
  return *scope.currentAccess;
}

VariableRef CompositionContext::insertVariable(std::string_view name, SHExposedTypeInfo type) {
  auto &scope = currentScope();

  bool wasExisting = findVariable(name);
  if (!wasExisting)
    scope.estimatedNumVariables++;

  auto newVar = insertAnonymousVariable(type);
  scope.variableMap[name] = newVar->id;
  return newVar;
}

ResolvedReferenceInfo CompositionContext::resolveReferenceInfo(const VariableAccessorChain &va) {
  ResolvedReferenceInfo res{};

  if (va.path.size() == 0) {
    throw std::logic_error("Invalid reference path");
  }

  // Find variable
  for (size_t i = 0; i < va.path.size(); i++) {
    auto &po = va.path[i];
    std::visit<void>(shards::Overload{
                         [&](const VA_Variable &v) {
                           auto var = findVariable(v.index);
                           res.exposedTypeInfo = var->exposed;
                           res.typeInfoPtr = &var->exposed.exposedType;
                           res.isMutable = var->exposed.isMutable;
                         },
                         [&](const VA_SubPath &p) {
                           if (!res.typeInfoPtr) {
                             throw std::logic_error(fmt::format("Invalid subpath without a valid base type in chain: {}", va));
                           }
                           auto subType = resolveVariableSubPath(*res.typeInfoPtr, p);
                           if (subType) {
                             res.typeInfoPtr = subType;
                           } else {
                             throw std::logic_error(fmt::format("Subpath not found: {}", po));
                           }
                         },
                         [&](const VA_Input &i) {
                           auto it =
                               std::find_if(stack.rbegin(), stack.rend(), [&](const auto &s) { return s->id == i.scopeId; });
                           if (it == stack.rend()) {
                             throw std::logic_error(fmt::format("Input scope not found: {}", i.scopeId));
                           }
                           Scope *scope = *it;
                           res.typeInfoPtr = &scope->originalInputType;
                           res.isMutable = false;
                         },
                         [&](const VA_WireOutput &wo) {
                           auto it = wireOutputTypes.find(wo.wireId);
                           if (it == wireOutputTypes.end()) {
                             throw std::logic_error(fmt::format("Wire output type not found: {}", wo.wireId));
                           }
                           res.typeInfoPtr = &it->second;
                           res.isMutable = false;
                         },
                     },
                     po.value);
  }
  return res;
}

TrackedReference &CompositionContext::trackReference(const VariableAccessorChain &va) {
  auto refGroupIt = rootReferenceMap.find(va.path[0]);
  if (refGroupIt != rootReferenceMap.end()) {
    TrackedReferenceGroup &refGroup = refGroupIt->second;
    auto it = refGroup.map.find(va);
    if (it != refGroup.map.end()) {
      return it->second;
    }
  } else {
    refGroupIt = rootReferenceMap.emplace(va.path[0], TrackedReferenceGroup{getAllocator()}).first;
  }

  auto &refGroup = refGroupIt->second;
  auto it = refGroup.map.find(va);
  if (it != refGroup.map.end()) {
    return it->second;
  }

  auto &ref = refGroup.map[va];
  return ref;
}

TrackedReference *CompositionContext::resolveReference(const VariableAccessorChain &va) {
  auto refGroupIt = rootReferenceMap.find(va.path[0]);
  if (refGroupIt != rootReferenceMap.end()) {
    TrackedReferenceGroup &refGroup = refGroupIt->second;
    auto it = refGroup.map.find(va);
    if (it != refGroup.map.end()) {
      return &it->second;
    }
  } else {
    refGroupIt = rootReferenceMap.emplace(va.path[0], TrackedReferenceGroup{getAllocator()}).first;
  }

  auto &refGroup = refGroupIt->second;
  auto it = refGroup.map.find(va);
  if (it != refGroup.map.end()) {
    return &it->second;
  }

  return nullptr;
}

const char *CompositionContext::tmpStr(std::string_view str) {
  char *bytes = (char *)tempAllocator.getAllocator()->allocate(str.size() + 1);
  memcpy(bytes, str.data(), str.size());
  bytes[str.size()] = 0;
  return bytes;
}

void CompositionContext::checkReferenceIsValid(VariableAccessorChain chain, size_t version) {
  // Resolve the references
  auto reference = resolveReference(chain);
  checkReferenceIsValidInternal(reference, chain, version);
}

void CompositionContext::checkReferenceIsValidInternal(TrackedReference *reference, VariableAccessorChain chain, size_t version) {
  if (!reference) {
    throw std::logic_error(fmt::format("Invalid reference path: {}", chain));
  }

  if (reference->version != version) {
    if (reference->lastInvalidatedBy) {
      throw std::logic_error(fmt::format("Reference is no longer valid here: {}, last invalidated by: {}", chain,
                                         shardContextStr(reference->lastInvalidatedBy->shard)));
    } else {
      throw std::logic_error(fmt::format("Reference is no longer valid here: {}", chain));
    }
  }
}

VariableRef CompositionContext::insertRefVariable(std::string_view name, const VersionedReference &vr, bool isMutable,
                                                  std::optional<SHTypeInfo> exposeAs) {
  // Resolve the references
  auto reference = resolveReference(vr.chain);
  checkReferenceIsValidInternal(reference, vr.chain, vr.version);

  SHExposedTypeInfo typeInfo{};
  typeInfo.isMutable = isMutable && reference->isMutable;
  typeInfo.exposedType = exposeAs ? *exposeAs : reference->cachedType;
  typeInfo.name = tmpStr(name);
  typeInfo.declared = true;

  auto &var = insertVariable(typeInfo.name, typeInfo).mutate();

  // Link this reference variable to it's reference
  var.referenceTarget.emplace(vr.chain);
  var.referenceVersion = reference->version;

  SPDLOG_LOGGER_DEBUG(logger, "Declared reference variable: {} (id: {}) mutable: {}, points to: {} (v: {}), type: {} in {}",
                      typeInfo.name, var.id, typeInfo.isMutable, *var.referenceTarget, var.referenceVersion, typeInfo.exposedType,
                      shardContextStr());

  return &var;
}

VariableRef CompositionContext::insertAnonymousVariable(SHExposedTypeInfo type) {
  auto &scope = currentScope();
  shassert(scope.wire && "Wire should be valid");
  uint32_t newId = uint32_t(variables.size());
  auto &newVar = variables.emplace_back(Variable{newId, scope.id, VariableKind::Local, type});
  newVar.exposed.internalId = newId | InternalIdFlagsInternal;
  currentShardInfo().variableRefs.push_back(newId);
  return &newVar;
}

compose::ShardInfo &CompositionContext::currentShardInfo() {
  auto &scope = currentScope();
  shassert(scope.wire && "Wire should be valid");
  auto currentShard = this->currentShard();
  if (currentShard) {
    shassert(currentShard->id != 0);
    auto it = scope.wire->shardSeqId.find(currentShard->id);
    if (it == scope.wire->shardSeqId.end()) {
      it = scope.wire->shardSeqId.emplace(currentShard->id, compose::ShardInfo{}).first;
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

void CompositionContext::assignWireComposeID(SHWire *wire) {
  if (wire->composeId == 0) {
    wire->composeId = idAllocator++;
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

    VariableAccessorChain va{getAllocator(), VariableAccessor::input(s.id)};
    annotateReferenceTo(va);
  }
  return s;
}
void CompositionContext::popScope() {
  auto scope = stack.back();

  VariableAccessorChain chain{getAllocator(), VariableAccessor::input(scope->id)};

  invalidateReferencePath(chain, InvalidationSource::ScopeExit);

  stack.pop_back();
  scopePool.push_back(scope);

  SPDLOG_LOGGER_DEBUG(logger, "Popped scope ({})", scope->id);
}

void CompositionContext::annotateSubPath(const SHVar &key) {
  auto va = VariableAccessor::key(key);
  auto &c = currentScope();
  if (!c.currentAccess) {
    SPDLOG_LOGGER_ERROR(logger, "Undefined variable access({}) (input missing)", va);
    flowClearUndeterministic();
    return;
  }

  auto &currentChain = c.currentAccess->chain;
  auto newChain = currentChain;
  newChain.append(va);

  SPDLOG_LOGGER_DEBUG(logger, "Annotating sub-path: {}", key);
  (void)annotateReferenceTo(newChain);
  flowTagAnnotated();
}

void CompositionContext::annotateRef(const VariableRef &variable) {
  if (!variable.isValid()) {
    throw std::logic_error("Invalid variable reference");
  }
  auto variableId = variable->id;
  auto va = VariableAccessorChain(getAllocator(), VariableAccessor::var(variableId, variable->exposed.name));

  SPDLOG_LOGGER_DEBUG(logger, "Annotating context variable: {} with type: {}", variable->exposed.name,
                      variable->exposed.exposedType);
  (void)annotateReferenceTo(va);
  flowTagAnnotated();
}

TrackedReference &CompositionContext::annotateReferenceTo(const VariableAccessorChain &va) {
  SPDLOG_LOGGER_DEBUG(logger, "Adding reference to: {}", va);
  auto &ref = trackReference(va);
  if (ref.version == 0) {
    // We initialize the reference here
    ++ref.version;
    SPDLOG_LOGGER_DEBUG(logger, "First reference to: {}", va);

    // Resolve the reference type here
    auto refInfo = resolveReferenceInfo(va);
    ref.cachedType = *refInfo.typeInfoPtr;
    ref.isMutable = refInfo.isMutable;
  }

  auto &s = currentScope();
  s.currentAccess.emplace(getAllocator(), va, ref.version);

  flowTagAnnotated();

  return ref;
}

void CompositionContext::annotateClearVariable() {
  flowTagAnnotated();
  flowClearVariable();
}
void CompositionContext::annotatePassthrough() {
  flowTagAnnotated();
  flowTagPassthrough();
}
void CompositionContext::annotateRebaseFlow() {
  flowTagAnnotated();

  // Rebase flow
  auto &c = currentScope();
  VariableAccessorChain va{getAllocator(), VariableAccessor::input(c.id)};
  annotateReferenceTo(va);
}

void CompositionContext::annotateWireOutput(SHWire *wire) {
  shassert(wire->composeId != 0 && "Wire compose id should be set");
  VariableAccessorChain va{getAllocator(), VariableAccessor::wireOutput(size_t(wire->composeId), wire->name)};
  wireOutputTypes[wire->composeId] = wire->outputType;
  // Invalidate all existing reference to this wire's output
  invalidateReferencePath(va);
  annotateReferenceTo(va);
}

void CompositionContext::invalidateWireOutput(SHWire *wire) {
  shassert(wire->composeId != 0 && "Wire compose id should be set");
  VariableAccessorChain va{getAllocator(), VariableAccessor::wireOutput(size_t(wire->composeId), wire->name)};
  invalidateReferencePath(va);
}

void CompositionContext::invalidateReferencePath(const VariableAccessorChain &va, InvalidationSource is) {
  if (va.path.size() == 0) {
    return;
  }

  auto &shardInfo = currentShardInfo();

  // Find the root container
  auto it = rootReferenceMap.find(va.path[0]);
  if (it == rootReferenceMap.end()) {
    return;
  }

  SPDLOG_LOGGER_DEBUG(logger, "Invalidating references with base path {}", va);

  for (auto &ref : it->second.map) {
    ref.second.version++;
    if (is == InvalidationSource::CurrentShard) {
      ref.second.lastInvalidatedBy = &shardInfo;
    } else {
      ref.second.lastInvalidatedBy = nullptr;
    }
    SPDLOG_LOGGER_DEBUG(logger, "Invalidating reference to {} (new version: {})", ref.first, ref.second.version);
  }
}

void CompositionContext::invalidateExposedReferences(const SHExposedTypesInfo &eti) {
  for (auto &info : eti) {
    auto var = findVariable(info.internalId);
    if (var && var->referenceTarget) {
      // Manually invalidate scoped reference
      auto &mvar = var.mutate();
      if (mvar.referenceVersion != size_t(~0)) {
        mvar.referenceVersion = size_t(~0);
        SPDLOG_LOGGER_DEBUG(logger, "Manually invalidating reference variable {} by {}", var->exposed.name, shardContextStr());
      }
    }
  }
}

VariableAccessorChain CompositionContext::getPathToVariable(const VariableRef &variable) {
  shassert(variable && "Variable must be valid");
  auto &c = currentScope();
  auto v = variable.variable;
  if (v->referenceTarget) {
    return *v->referenceTarget;
  }
  return VariableAccessorChain{getAllocator(), VariableAccessor::var(v->id, v->exposed.name)};
}

void CompositionContext::flowAnnotateNextShard(Shard *shard) {
  auto &c = currentScope();
  c.shardName = shard->name(shard);
  c.annotations.reset();

  // Force creation of shard info/seqid
  currentShardInfo();
  SPDLOG_LOGGER_DEBUG(logger, "Annotating next shard: {} (id: {}, line: {}, column: {})", c.shardName, c.bottom->id,
                      c.bottom->line, c.bottom->column);
}

void CompositionContext::flowClearVariable() {
  auto &v = currentScope().currentAccess;
  if (v) {
    SPDLOG_LOGGER_DEBUG(logger, "Clearing variable: {}", *v);
  }
  v.reset();
}

void CompositionContext::flowClearUndeterministic() {
  auto &c = currentScope();
  c.currentAccess.reset();
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

      ctx.assignWireComposeID(sourceWire);

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

        auto &v =
            ctx.insertVariable(key.payload.stringValue, SHExposedTypeInfo{key.payload.stringValue, {}, *type, true /* mutable */})
                .mutate();
        v.kind = VariableKind::External;
        v.exposed.trackingMask = var.trackingMask;
      }

      // add present mesh variables as well if we have a mesh
      auto mesh = scope.wire->source->mesh.lock();
      if (mesh) {
        for (auto &v : mesh->getVariables()) {
          // only add variables with metadata basically
          auto metadata = mesh->getMetadata(&v.second);
          if (metadata) {
            std::string_view sName(v.first.payload.stringValue, v.first.payload.stringLen);
            auto &v = ctx.insertVariable(sName, *metadata).mutate();
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
          auto &v = ctx.insertVariable(v1->exposed.name, v1->exposed).mutate();
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
          if (ctx.stack.size() > 1) {
            SPDLOG_LOGGER_TRACE(compose::logger, "Internally scoped variable '{}' declared in shard {}",
                                data.shared.elements[i].name, ctx.shardContextStr(1));
          } else {
            SPDLOG_LOGGER_TRACE(compose::logger, "Internally defined variable '{}' declared to wire {} composition",
                                data.shared.elements[i].name, scope.wireName());
          }
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
      static constexpr uint32_t IdNone = 0;
      static constexpr uint32_t IdFlagsExternal = 1 << 31;
      static constexpr uint32_t IdFlagsInherited = 1 << 30;
      static constexpr uint32_t IdFlagsGlobal = 1 << 29;
      static constexpr uint32_t IdFlagsRef = 1 << 28;
      static constexpr uint32_t IdFlagMask = IdFlagsExternal | IdFlagsInherited | IdFlagsGlobal | IdFlagsRef;
      static constexpr uint32_t IdValueMask = IdFlagsRef - 1;

      // Finalize and populate wire variable data
      SPDLOG_LOGGER_DEBUG(compose::logger, "Wire {} analysis", scope.wireName());
      pmr::set<size_t> allRequiredVariables(ctx.getAllocator());
      pmr::map<size_t, compose::ShardInfo *> orderedShards(ctx.getAllocator());
      pmr::vector<WireRuntimeVariableInfo::VariableDecl> localVariables(ctx.getAllocator());
      pmr::vector<WireRuntimeVariableInfo::VariableDecl> refVariables(ctx.getAllocator());
      pmr::vector<WireRuntimeVariableInfo::VariableDecl> inheritedVariables(ctx.getAllocator());
      pmr::vector<WireRuntimeVariableInfo::VariableDecl> externalVariables(ctx.getAllocator());
      pmr::vector<WireRuntimeVariableInfo::VariableDecl> globalVariables(ctx.getAllocator());
      pmr::unordered_map<size_t, size_t> variableRemapping(ctx.getAllocator());
      pmr::unordered_map<size_t, size_t> reverseRemapping(ctx.getAllocator());
      using VarDecl = WireRuntimeVariableInfo::VariableDecl;
      for (auto &[k, v] : composeWireRoot->shardSeqId) {
        orderedShards.emplace(v.seqId, &v);
      }

      // First pass: collect required & references wire variables
      for (auto &[k, shardInfo] : orderedShards) {
        // Ignore wire root (seqid == 0) since it's only used to populate inherited variables, and we only want to collect
        // usages
        if (k == 0) {
          // bool okay = true;
          // for (auto &v : shardInfo->variableRefs) {
          //   auto &var = ctx.variables[v];
          //   if (var.kind == VariableKind::Local) {
          //     okay = false;
          //     SPDLOG_LOGGER_ERROR(compose::logger, "Wire root must not have local variables, found {} (id: {}, type: {})",
          //                         var.exposed.name, var.id, var.exposed.exposedType);
          //   }
          // }
          // if (!okay) {
          //   throw std::logic_error("Wire root must not have local variables");
          // }
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
              size_t newVarId = inheritedVariables.size() | IdFlagsInherited;
              vs.variableLookup[var.exposed.name] = newVarId;
              inheritedVariables.emplace_back(std::move(vd));
              targetIt = variableRemapping.emplace(v, newVarId).first;

              allRequiredVariables.insert(v);
            } else if (var.kind == VariableKind::External) {
              size_t newVarId = externalVariables.size() | IdFlagsExternal;
              vs.variableLookup[var.exposed.name] = newVarId;
              externalVariables.emplace_back(std::move(vd));
              targetIt = variableRemapping.emplace(v, newVarId).first;
            } else if (var.kind == VariableKind::Global) {
              size_t newVarId = globalVariables.size() | IdFlagsGlobal;
              vs.variableLookup[var.exposed.name] = newVarId;
              globalVariables.emplace_back(std::move(vd));
              targetIt = variableRemapping.emplace(v, newVarId).first;
            } else if (var.kind == VariableKind::Local && var.referenceTarget) {
              size_t newVarId = refVariables.size() | IdFlagsRef;
              vs.variableLookup[var.exposed.name] = newVarId;
              refVariables.emplace_back(std::move(vd));
              targetIt = variableRemapping.emplace(v, newVarId).first;
            } else {
              shassert(var.kind == VariableKind::Local);
              size_t newVarId = localVariables.size();
              vs.variableLookup[var.exposed.name] = newVarId;
              localVariables.emplace_back(std::move(vd));
              targetIt = variableRemapping.emplace(v, newVarId).first;
            }
            reverseRemapping.emplace(targetIt->second, targetIt->first);
          }

          auto &vs = runtimeVariableInfo->variableScopes.emplace(shardInfo->seqId, WireRuntimeVariableInfo::VariableScope{})
                         .first->second;
          vs.variableLookup[var.exposed.name] = targetIt->second;
        }

        // Populate wire->requirements
        if (data.wire) {
          // TODO
          // auto &outReqs = data.wire->requirements;
          // outReqs.clear();
          // for (auto &req : allRequiredVariables) {
          //   auto &v = ctx.variables[req];
          //   outReqs.emplace(v.exposed.name, v.exposed);
          // }
        }
      }

      runtimeVariableInfo->numExternalVariables = externalVariables.size();
      runtimeVariableInfo->numInheritedVariables = inheritedVariables.size();
      runtimeVariableInfo->numRefVariables = refVariables.size();
      runtimeVariableInfo->numLocalVariables = localVariables.size();
      runtimeVariableInfo->numGlobalVariables = globalVariables.size();

      for (auto &v : localVariables) {
        runtimeVariableInfo->variables.emplace_back(std::move(v));
      }
      for (auto &v : refVariables) {
        runtimeVariableInfo->variables.emplace_back(std::move(v));
      }
      for (auto &v : inheritedVariables) {
        runtimeVariableInfo->variables.emplace_back(std::move(v));
      }
      for (auto &v : externalVariables) {
        runtimeVariableInfo->variables.emplace_back(std::move(v));
      }
      for (auto &v : globalVariables) {
        runtimeVariableInfo->variables.emplace_back(std::move(v));
      }

      // For debug info, etc.
      pmr::unordered_map<size_t, VariableRef> mapToComposeVariable(ctx.getAllocator());

      // Fixup scope references
      for (auto &[k, v] : runtimeVariableInfo->variableScopes) {
        for (auto &[name, id] : v.variableLookup) {
          auto srcIdx = reverseRemapping.find(id);

          if (id & IdFlagsRef) {
            id = (id & IdValueMask) + runtimeVariableInfo->refOffset();
          } else if (id & IdFlagsInherited) {
            id = (id & IdValueMask) + runtimeVariableInfo->inheritedOffset();
          } else if (id & IdFlagsExternal) {
            id = (id & IdValueMask) + runtimeVariableInfo->externalOffset();
          } else if (id & IdFlagsGlobal) {
            id = (id & IdValueMask) + runtimeVariableInfo->globalOffset();
          }

          if (srcIdx != reverseRemapping.end()) {
            reverseRemapping.erase(srcIdx);
            mapToComposeVariable.emplace(id, &ctx.variables[srcIdx->second]);
          }
        }
      }

      for (auto &v : allRequiredVariables) {
        auto &var = ctx.variables[v];
        SPDLOG_LOGGER_DEBUG(compose::logger, " Wire required variable: {} (id: {}, type: {})", var.exposed.name, var.id,
                            var.exposed.exposedType);
      }

      SPDLOG_LOGGER_DEBUG(compose::logger, "Wire {} variables ({} total)", scope.wireName(),
                          runtimeVariableInfo->variables.size());
      for (size_t i = 0; i < runtimeVariableInfo->variables.size(); i++) {
        auto& composeVar = mapToComposeVariable.find(i)->second;
        SPDLOG_LOGGER_DEBUG(compose::logger, "  [{}] ({}) {} (id: {}, type: {})", i, magic_enum::enum_name(composeVar->kind), composeVar->exposed.name, composeVar->id, (const SHTypeInfo &)runtimeVariableInfo->variables[i].type);
      }

      runtimeVariableInfo->initStorage();
      SPDLOG_LOGGER_DEBUG(compose::logger, "Wire {} analysis done", scope.wireName());
    }

    return result;
  } catch (...) {
    if (runtimeVariableInfo) {
      data.wire->runtimeVariableInfo.reset();
    }
    throw;
  }
}

bool typeRequiresInvalidationWhenUpdated(const SHTypeInfo &t) {
  // Table can remove keys invalidating references
  // Ignore fixed struct table
  if (t.basicType == SHType::Table && !t.table.fixedStructTable)
    return true;
  // Seq can resize due to assignment, invalidating reference
  if (t.basicType == SHType::Seq)
    return true;
  // This might be seq/table, no way to know during compose
  if (t.basicType == SHType::Any)
    return true;
  // Any other type can stay valid as a reference
  return false;
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
