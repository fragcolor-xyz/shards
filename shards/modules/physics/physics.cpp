#include <shards/core/shared.hpp>
#include <shards/core/params.hpp>
#include <shards/core/object_var_util.hpp>
#include <shards/common_types.hpp>
#include <shards/gfx/types.hpp>
#include <gfx/hasherxxh3.hpp>
#include <gfx/drawable.hpp>
#include <tracy/Wrapper.hpp>
#include "physics.hpp"
#include "many_container.hpp"
#include "core.hpp"

#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>

namespace shards::Physics {
std::atomic_uint64_t Node::UidCounter = 0;
std::atomic_uint64_t Core::UidCounter = 0;
std::atomic_uint64_t ShardsShape::UidCounter;

struct ContextShard {
  static SHTypesInfo inputTypes() { return shards::CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return ShardsContext::Type; }
  static SHOptionalString help() { return SHCCSTR(""); }

  PARAM_IMPL();

  ShardsContext *_context{};

  void warmup(SHContext *context) {
    PARAM_WARMUP(context);
    shassert(!_context);
    _context = ShardsContext::ObjectVar.New();
    _context->core = std::make_shared<Core>();
    _context->core->init();
  }
  void cleanup(SHContext *context) {
    PARAM_CLEANUP(context);
    if (_context) {
      ShardsContext::ObjectVar.Release(_context);
      _context = nullptr;
    }
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
    auto &context = varAsObjectChecked<ShardsContext>(_context.get(), ShardsContext::Type);
    context.core->end();
    context.core->simulate();
    // context.core->
    // for (auto &node : context.core->getNodeMap()) {
    //   auto& nodeData = node.second;
    //   context.physicsSystem.
    // }

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
    if (contextVar) {
      releaseVariable(contextVar);
      contextVar = nullptr;
    }
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

DECL_ENUM_INFO(JPH::EAllowedDOFs, PhysicsDOF, 'phDf');
DECL_ENUM_INFO(JPH::EMotionType, PhysicsMotion, 'phMo');

enum ParamTypeMask : uint32_t {
  PTM_None = 0,
  PTM_Friction = 1 << 0,
  PTM_Restitution = 1 << 1,
  PTM_LinearDamping = 1 << 2,
  PTM_AngularDamping = 1 << 3,
  PTM_MaxLinearVelocity = 1 << 4,
  PTM_MaxAngularVelocity = 1 << 5,
  PTM_GravityFactor = 1 << 6,
  PTM_AllowedDOFs = 1 << 7,
  PTM_MotionType = 1 << 8,
};
static ParamTypeMask &operator|=(ParamTypeMask &lhs, ParamTypeMask rhs) {
  return lhs = static_cast<ParamTypeMask>(static_cast<uint32_t>(lhs) | static_cast<uint32_t>(rhs));
}

struct BodyShard {
  static SHTypesInfo inputTypes() { return shards::CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return ShardsNode::Type; }
  static SHOptionalString help() { return SHCCSTR("Defines a new node"); }

  static inline Type PhysicsDOFSeqType = Type::SeqOf(PhysicsDOFEnumInfo::Type);
  static inline Types PhysicsDOFTypes{PhysicsDOFEnumInfo::Type, PhysicsDOFSeqType, Type::VariableOf(PhysicsDOFSeqType)};

  PARAM_PARAMVAR(_location, "Location", "The initial location, updated by physics simulation", {shards::CoreInfo::Float3VarType});
  PARAM_PARAMVAR(_rotation, "Rotation", "The initial location, updated by physics simulation", {shards::CoreInfo::Float4VarType});
  PARAM_VAR(_static, "Static", "Static node, persist when not activated", {shards::CoreInfo::BoolType});
  PARAM_PARAMVAR(_enabled, "Enabled", "Can be used to toggle this node when it has static persistence",
                 {shards::CoreInfo::BoolType, shards::CoreInfo::BoolVarType});
  PARAM_PARAMVAR(_shape, "Shape", "The shape of the body", {ShardsShape::VarType});
  PARAM_PARAMVAR(_friction, "Friction", "", {shards::CoreInfo::FloatType, shards::CoreInfo::FloatVarType});
  PARAM_PARAMVAR(_restitution, "Restitution", "Restitution coefficient",
                 {shards::CoreInfo::FloatType, shards::CoreInfo::FloatVarType});
  PARAM_PARAMVAR(_linearDamping, "LinearDamping", "Linear damping coefficient",
                 {shards::CoreInfo::FloatType, shards::CoreInfo::FloatVarType});
  PARAM_PARAMVAR(_angularDamping, "AngularDamping", "Angular damping coefficient",
                 {shards::CoreInfo::FloatType, shards::CoreInfo::FloatVarType});
  PARAM_PARAMVAR(_maxLinearVelocity, "MaxLinearVelocity", "Max linear velocity",
                 {shards::CoreInfo::FloatType, shards::CoreInfo::FloatVarType});
  PARAM_PARAMVAR(_maxAngularVelocity, "MaxAngularVelocity", "Max angular velocity",
                 {shards::CoreInfo::FloatType, shards::CoreInfo::FloatVarType});
  PARAM_PARAMVAR(_gravityFactor, "GravityFactor", "Gravity factor",
                 {shards::CoreInfo::FloatType, shards::CoreInfo::FloatVarType});
  PARAM_PARAMVAR(_allowedDOFs, "AllowedDOFs", "Allowed degrees of freedom", PhysicsDOFTypes);
  PARAM_PARAMVAR(_motionType, "MotionType", "Motion type",
                 {PhysicsMotionEnumInfo::Type, Type::VariableOf(PhysicsMotionEnumInfo::Type)});
  PARAM_PARAMVAR(_context, "Context", "The physics context", {ShardsContext::VarType});
  PARAM_IMPL(PARAM_IMPL_FOR(_location), PARAM_IMPL_FOR(_rotation), PARAM_IMPL_FOR(_static), PARAM_IMPL_FOR(_enabled),
             PARAM_IMPL_FOR(_shape), PARAM_IMPL_FOR(_friction), PARAM_IMPL_FOR(_restitution), PARAM_IMPL_FOR(_linearDamping),
             PARAM_IMPL_FOR(_angularDamping), PARAM_IMPL_FOR(_maxLinearVelocity), PARAM_IMPL_FOR(_maxAngularVelocity),
             PARAM_IMPL_FOR(_gravityFactor), PARAM_IMPL_FOR(_allowedDOFs), PARAM_IMPL_FOR(_motionType), PARAM_IMPL_FOR(_context));

  ParamTypeMask _dynamicParameterMask = PTM_None;

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

  BodyShard() {
    _static = Var(false);
    _enabled = Var(true);
    _context = Var::ContextVar(ShardsContext::VariableName);

    // Defaults copied from BodyCreationSettings
    _friction = Var(0.2f);
    _restitution = Var(0.0f);
    _linearDamping = Var(0.05f);
    _angularDamping = Var(0.05f);
    _maxLinearVelocity = Var(500.0f);
    _maxAngularVelocity = Var(0.25f * gfx::pi * 60.0f);
    _gravityFactor = Var(1.0f);
    _allowedDOFs = Var::Enum(JPH::EAllowedDOFs::All, PhysicsDOFEnumInfo::Type);
    _motionType = Var::Enum(JPH::EMotionType::Dynamic, PhysicsMotionEnumInfo::Type);
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

    // Determine dynamic parameters
    if (_friction.isVariable())
      _dynamicParameterMask |= PTM_Friction;
    if (_restitution.isVariable())
      _dynamicParameterMask |= PTM_Restitution;
    if (_linearDamping.isVariable())
      _dynamicParameterMask |= PTM_LinearDamping;
    if (_angularDamping.isVariable())
      _dynamicParameterMask |= PTM_AngularDamping;
    if (_maxLinearVelocity.isVariable())
      _dynamicParameterMask |= PTM_MaxLinearVelocity;
    if (_maxAngularVelocity.isVariable())
      _dynamicParameterMask |= PTM_MaxAngularVelocity;
    if (_gravityFactor.isVariable())
      _dynamicParameterMask |= PTM_GravityFactor;
    if (_allowedDOFs.isVariable())
      _dynamicParameterMask |= PTM_AllowedDOFs;
    if (_motionType.isVariable())
      _dynamicParameterMask |= PTM_MotionType;

    return outputTypes().elements[0];
  }

  JPH::EAllowedDOFs convertAllowedDOFs(const SHVar &input) {
    if (input.valueType != SHType::Seq) {
      return (JPH::EAllowedDOFs)input.payload.enumValue;
    } else {
      JPH::EAllowedDOFs dofs = JPH::EAllowedDOFs::None;
      for (auto &dof : input) {
        dofs |= (JPH::EAllowedDOFs)dof.payload.enumValue;
      }
      return dofs;
    }
  }

  void initNewNode(std::shared_ptr<Node> node) {
    if ((bool)*_static)
      node->persistence = true;
    node->enabled = _enabled.get().payload.boolValue;

    node->params.friction = _friction.get().payload.floatValue;
    node->params.resitution = _restitution.get().payload.floatValue;
    node->params.linearDamping = _linearDamping.get().payload.floatValue;
    node->params.angularDamping = _angularDamping.get().payload.floatValue;

    node->params.maxLinearVelocity = _maxLinearVelocity.get().payload.floatValue;
    node->params.maxAngularVelocity = _maxAngularVelocity.get().payload.floatValue;
    node->params.gravityFactor = _gravityFactor.get().payload.floatValue;
    node->params.allowedDofs = convertAllowedDOFs(_allowedDOFs.get());
    node->params.motionType = (JPH::EMotionType)_motionType.get().payload.enumValue;

    node->location = toFloat3(_location.get().payload.float3Value);
    node->rotation = toQuat(_rotation.get().payload.float4Value);
    node->updateParamHash0();
    node->updateParamHash1();
  }

  SHVar activate(SHContext *shContext, const SHVar &input) {
    ZoneScopedN("Physics::entity");

    auto &manyItem = _manyInstances.pull(       //
        _requiredContext->core->frameCounter(), //
        [&]() {                                 //
          Instance inst;
          inst.node->node = _requiredContext->core->createNewNode();
          initNewNode(inst.node->node);
          return inst;
        });

    auto &innerNode = manyItem.node->node;

    innerNode->enabled = _enabled.get().payload.boolValue;
    _requiredContext->core->touchNode(innerNode);

    // Filtering to avoid unnecessary checks for static parameters
    if ((_dynamicParameterMask & 0b1111) != 0) {
      if (_dynamicParameterMask & PTM_Friction) {
        innerNode->params.friction = _friction.get().payload.floatValue;
      }
      if (_dynamicParameterMask & PTM_Restitution) {
        innerNode->params.resitution = _restitution.get().payload.floatValue;
      }
      if (_dynamicParameterMask & PTM_LinearDamping) {
        innerNode->params.linearDamping = _linearDamping.get().payload.floatValue;
      }
      if (_dynamicParameterMask & PTM_AngularDamping) {
        innerNode->params.angularDamping = _angularDamping.get().payload.floatValue;
      }
      innerNode->updateParamHash0();
    }
    if ((_dynamicParameterMask & 0b111110000) != 0) {
      if (_dynamicParameterMask & PTM_MaxLinearVelocity) {
        innerNode->params.maxLinearVelocity = _maxLinearVelocity.get().payload.floatValue;
      }
      if (_dynamicParameterMask & PTM_MaxAngularVelocity) {
        innerNode->params.maxAngularVelocity = _maxAngularVelocity.get().payload.floatValue;
      }
      if (_dynamicParameterMask & PTM_GravityFactor) {
        innerNode->params.gravityFactor = _gravityFactor.get().payload.floatValue;
      }
      if (_dynamicParameterMask & PTM_AllowedDOFs) {
        innerNode->params.allowedDofs = convertAllowedDOFs(_allowedDOFs.get());
      }
      if (_dynamicParameterMask & PTM_MotionType) {
        innerNode->params.motionType = (JPH::EMotionType)_motionType.get().payload.enumValue;
      }
      innerNode->updateParamHash1();
    }

    auto &shardsShape = varAsObjectChecked<ShardsShape>(_shape.get(), ShardsShape::Type);
    innerNode->shape = shardsShape.shape;
    innerNode->shapeUid = shardsShape.uid;

    assignVariableValue(_location.get(), toVar(innerNode->location));
    assignVariableValue(_rotation.get(), toVar(innerNode->rotation));

    return manyItem.var;
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
    TracyMessageL("Physics::dump");

    auto &core = _requiredContext->core;
    SPDLOG_INFO("Physics Core ({}):", core->getId());
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

// Callback for traces, connect this to your own trace function if you have one
static void TraceImpl(const char *inFMT, ...) {
  // Format the message
  va_list list;
  va_start(list, inFMT);
  char buffer[1024];
  vsnprintf(buffer, sizeof(buffer), inFMT, list);
  va_end(list);
  SPDLOG_LOGGER_TRACE(getLogger(), "{}", buffer);
}

#ifdef JPH_ENABLE_ASSERTS
// Callback for asserts, connect this to your own assert handler if you have one
static bool AssertFailedImpl(const char *inExpression, const char *inMessage, const char *inFile, uint32_t inLine) {
  getLogger()->log(spdlog::source_loc{inFile, (int)inLine, inExpression}, spdlog::level::err,
                   (inMessage != nullptr ? inMessage : ""));
  // Breakpoint
  return true;
};
#endif // JPH_ENABLE_ASSERTS
} // namespace shards::Physics

SHARDS_REGISTER_FN(physics) {
  using namespace shards::Physics;

  JPH::RegisterDefaultAllocator();
  JPH::Trace = TraceImpl;
  JPH_IF_ENABLE_ASSERTS(JPH::AssertFailed = AssertFailedImpl;)
  JPH::Factory::sInstance = new JPH::Factory();
  JPH::RegisterTypes();

  REGISTER_ENUM(PhysicsDOFEnumInfo);
  REGISTER_ENUM(PhysicsMotionEnumInfo);
  REGISTER_SHARD("Physics.Context", ContextShard);
  REGISTER_SHARD("Physics.End", EndContextShard);
  REGISTER_SHARD("Physics.WithContext", WithContextShard);
  REGISTER_SHARD("Physics.Body", BodyShard);
  REGISTER_SHARD("Physics.Dump", DumpShard);
}