#include <shards/core/shared.hpp>
#include <shards/core/params.hpp>
#include <shards/core/object_var_util.hpp>
#include <shards/common_types.hpp>
#include <tracy/Wrapper.hpp>
#include "regent.hpp"

namespace shards::regent {

struct ShardsContext {
  static inline int32_t ObjectId = 'reCx';
  static inline const char VariableName[] = "Regent.Context";
  static inline shards::Type Type = Type::Object(CoreCC, ObjectId);
  static inline SHTypeInfo RawType = Type;
  static inline shards::Type VarType = Type::VariableOf(Type);

  static inline shards::ObjectVar<ShardsContext, nullptr, nullptr, nullptr, true> ObjectVar{VariableName, RawType.object.vendorId,
                                                                                            RawType.object.typeId};

  std::shared_ptr<Core> core;
};

struct ShardsNode {
  static inline int32_t ObjectId = 'reNo';
  static inline const char VariableName[] = "Regent.Node";
  static inline shards::Type Type = Type::Object(CoreCC, ObjectId);
  static inline SHTypeInfo RawType = Type;
  static inline shards::Type VarType = Type::VariableOf(Type);

  static inline shards::ObjectVar<ShardsNode, nullptr, nullptr, nullptr, true> ObjectVar{VariableName, RawType.object.vendorId,
                                                                                         RawType.object.typeId};

  std::shared_ptr<Node> node;
};

using RequiredContext = RequiredContextVariable<ShardsContext, ShardsContext::RawType, ShardsContext::VariableName, true>;

struct ContextShard {
  static SHTypesInfo inputTypes() { return shards::CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return ShardsContext::Type; }
  static SHOptionalString help() { return SHCCSTR(""); }

  PARAM_IMPL();

  ShardsContext *_context;

  void warmup(SHContext *context) {
    PARAM_WARMUP(context);
    _context = ShardsContext::ObjectVar.New();
    _context->core = std::make_shared<Core>();
  }
  void cleanup(SHContext *context) {
    PARAM_CLEANUP(context);
    ShardsContext::ObjectVar.Release(_context);
  }

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    return outputTypes().elements[0];
  }

  SHVar activate(SHContext *shContext, const SHVar &input) {
    _context->core->begin();
    return ShardsContext::ObjectVar.Get(_context);
  }
};

struct EndContextShard {
  static SHTypesInfo inputTypes() { return shards::CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return shards::CoreInfo::AnyType; }
  static SHOptionalString help() { return SHCCSTR(""); }

  PARAM_PARAMVAR(_context, "Context", "The context", {ShardsContext::VarType});
  PARAM_IMPL(PARAM_IMPL_FOR(_context));

  void warmup(SHContext *context) { PARAM_WARMUP(context); }
  void cleanup(SHContext *context) { PARAM_CLEANUP(context); }

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    return outputTypes().elements[0];
  }

  SHVar activate(SHContext *shContext, const SHVar &input) {
    varAsObjectChecked<ShardsContext>(_context.get(), ShardsContext::Type).core->end();
    return input;
  }
};

struct WithContextShard {
  static SHTypesInfo inputTypes() { return shards::CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return shards::CoreInfo::AnyType; }
  static SHOptionalString help() { return SHCCSTR(""); }

  PARAM(ShardsVar, _contents, "Contents", "The contents to run with the context in scope", {shards::CoreInfo::ShardSeqOrNone});
  PARAM_PARAMVAR(_context, "Context", "The context", {ShardsContext::VarType});
  PARAM_IMPL(PARAM_IMPL_FOR(_contents), PARAM_IMPL_FOR(_context));

  SHVar *contextVar{};

  void warmup(SHContext *context) {
    PARAM_WARMUP(context);
    contextVar = referenceVariable(context, ShardsContext::VariableName);
  }
  void cleanup(SHContext *context) {
    PARAM_CLEANUP(context);
    releaseVariable(contextVar);
  }

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    ExposedInfo innerShared{data.shared};
    innerShared.push_back(SHExposedTypeInfo{
        .name = ShardsContext::VariableName,
        .exposedType = ShardsContext::RawType,
        .isMutable = false,
        .isProtected = true,
    });
    SHInstanceData innerData = data;
    innerData.shared = SHExposedTypesInfo(innerShared);
    _contents.compose(innerData);
    return outputTypes().elements[0];
  }

  SHVar activate(SHContext *shContext, const SHVar &input) {
    withObjectVariable(*contextVar, _context.get().payload.objectValue, ShardsContext::Type, [&]() {
      SHVar _dummy{};
      _contents.activate(shContext, input, _dummy);
    });
    return input;
  }
};

struct EntityShard {
  static SHTypesInfo inputTypes() { return shards::CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return ShardsNode::Type; }
  static SHOptionalString help() { return SHCCSTR("Defines a new node"); }

  PARAM_VAR(_static, "Static", "Static geometry", {shards::CoreInfo::BoolType});
  PARAM_PARAMVAR(_enabled, "Enabled", "Enable this node", {shards::CoreInfo::BoolType, shards::CoreInfo::BoolVarType});
  PARAM_PARAMVAR(_context, "Context", "The context", {ShardsContext::VarType});
  PARAM_IMPL(PARAM_IMPL_FOR(_static), PARAM_IMPL_FOR(_enabled), PARAM_IMPL_FOR(_context));

  struct Instance {
    OwnedVar var;
    ShardsNode *node;

    Instance() {
      auto [node, var] = ShardsNode::ObjectVar.NewOwnedVar();
      this->var = std::move(var);
      this->node = &node;
    }
    Instance(Instance &&other) = default;
    Instance &operator=(Instance &&other) = default;
  };
  ManyContainer<Instance> _manyInstances;
  RequiredContext _requiredContext;

  EntityShard() {
    _static = Var(false);
    _enabled = Var(true);
    _context = Var::ContextVar(ShardsContext::VariableName);
  }

  void warmup(SHContext *context) {
    PARAM_WARMUP(context);
    _requiredContext.warmup(context, &_context);
  }

  void cleanup(SHContext *context) {
    _manyInstances.clear();
    _requiredContext.cleanup(context);
    PARAM_CLEANUP(context);
  }

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    _requiredContext.compose(data, _requiredVariables);
    return outputTypes().elements[0];
  }

  SHVar activate(SHContext *shContext, const SHVar &input) {
    ZoneScopedN("Regent::entity");

    auto &instance = _manyInstances.pull(       //
        _requiredContext->core->frameCounter(), //
        [&]() {                                 //
          Instance inst;
          inst.node->node = _requiredContext->core->createNewNode();
          if ((bool)*_static) {
            inst.node->node->persistence = true;
          }
          return inst;
        });
    instance.node->node->enabled = _enabled.get().payload.boolValue;
    _requiredContext->core->touchNode(instance.node->node);

    return instance.var;
  }
};

struct DumpShard {
  static SHTypesInfo inputTypes() { return shards::CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return shards::CoreInfo::AnyType; }
  static SHOptionalString help() { return SHCCSTR(""); }

  PARAM_PARAMVAR(_context, "Context", "The context", {ShardsContext::VarType});
  PARAM_IMPL(PARAM_IMPL_FOR(_context));

  RequiredContext _requiredContext;

  void warmup(SHContext *context) {
    PARAM_WARMUP(context);
    _requiredContext.warmup(context, &_context);
  }
  void cleanup(SHContext *context) {
    _requiredContext.cleanup(context);
    PARAM_CLEANUP(context);
  }

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    _requiredContext.compose(data, _requiredVariables, &_context);
    return data.inputType;
  }

  SHVar activate(SHContext *shContext, const SHVar &input) {
    TracyMessageL("Regent::dump");

    auto &core = _requiredContext->core;
    SPDLOG_INFO("Regent Core ({}):", core->getId());
    const auto &nodeMap = core->getNodeMap();
    SPDLOG_INFO(" Nodes: {}", nodeMap.size());
    for (auto &node : nodeMap) {
      SPDLOG_INFO("  Node: {}({}) LT: {}", node.first->uid, (void *)node.first, node.second.lastTouched);
    }
    SPDLOG_INFO(" Active Nodes: {}", core->getActiveNodes().size());
    for (auto &node : core->getActiveNodes()) {
      SPDLOG_INFO("  Node: {}({})", node->node->uid, (void *)node->node.get());
    }
    return input;
  }
};

} // namespace shards::regent

SHARDS_REGISTER_FN(regent) {
  using namespace shards::regent;
  REGISTER_SHARD("Regent.Context", ContextShard);
  REGISTER_SHARD("Regent.End", EndContextShard);
  REGISTER_SHARD("Regent.WithContext", WithContextShard);
  REGISTER_SHARD("Regent.Entity", EntityShard);
  REGISTER_SHARD("Regent.Dump", DumpShard);
}