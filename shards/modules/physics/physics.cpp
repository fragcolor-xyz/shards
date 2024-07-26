#include <shards/core/shared.hpp>
#include <shards/core/params.hpp>
#include <shards/core/object_var_util.hpp>
#include <shards/common_types.hpp>
#include <shards/linalg_shim.hpp>
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
std::atomic_uint64_t SHShape::UidCounter;

struct ContextShard {
  static SHTypesInfo inputTypes() { return shards::CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return ShardsContext::Type; }
  static SHOptionalString help() {
    return SHCCSTR("The core of the physics system, needs to be activated at the start of the simulation");
  }

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
  static SHOptionalString help() { return SHCCSTR("Runs physics simulation, should be run after defining physics bodies"); }

  PARAM_PARAMVAR(_context, "Context", "The context", {ShardsContext::VarType});
  PARAM_PARAMVAR(_timeStep, "TimeStep", "Time to simulate", {CoreInfo::FloatType, CoreInfo::FloatVarType});
  PARAM_PARAMVAR(_maxIterations, "MaxIterations", "Maximum number of iterations to run the simulation in",
                 {CoreInfo::IntType, CoreInfo::IntVarSeqType});
  PARAM_IMPL(PARAM_IMPL_FOR(_context), PARAM_IMPL_FOR(_timeStep), PARAM_IMPL_FOR(_maxIterations));

  EndContextShard() {
    _timeStep = Var(1.0f / 60.0f);
    _maxIterations = Var(1);
  }

  void warmup(SHContext *context) { PARAM_WARMUP(context); }
  void cleanup(SHContext *context) { PARAM_CLEANUP(context); }

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    return outputTypes().elements[0];
  }

  SHVar activate(SHContext *shContext, const SHVar &input) {
    double timeStep = _timeStep.get().payload.floatValue;
    auto numIterations = _maxIterations.get().payload.intValue;

    if (numIterations < 1 || numIterations > 128)
      throw std::runtime_error("MaxIterations must be at least 1 and at most 128");
    if (timeStep <= 0.0)
      return input; // NOOP

    auto &context = varAsObjectChecked<ShardsContext>(_context.get(), ShardsContext::Type);
    context.core->end();
    context.core->simulate(timeStep, numIterations);

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
    contextVar = referenceVariable(context, ShardsContext::VariableName);
    PARAM_WARMUP(context);
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
  PTM_CollisionGroup = 1 << 9,
};

static ParamTypeMask &operator|=(ParamTypeMask &lhs, ParamTypeMask rhs) {
  return lhs = static_cast<ParamTypeMask>(static_cast<uint32_t>(lhs) | static_cast<uint32_t>(rhs));
}

struct BodyShard {
  static SHTypesInfo inputTypes() { return shards::CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return SHBody::Type; }
  static SHOptionalString help() { return SHCCSTR("Defines a new node"); }

  static inline Type PhysicsDOFSeqType = Type::SeqOf(PhysicsDOFEnumInfo::Type);
  static inline Types PhysicsDOFTypes{PhysicsDOFEnumInfo::Type, PhysicsDOFSeqType, Type::VariableOf(PhysicsDOFSeqType)};

  PARAM_PARAMVAR(_location, "Location", "The initial location, updated by physics simulation", {shards::CoreInfo::Float3VarType});
  PARAM_PARAMVAR(_rotation, "Rotation", "The initial location, updated by physics simulation", {shards::CoreInfo::Float4VarType});
  PARAM_VAR(_static, "Static", "Static node, persist when not activated", {shards::CoreInfo::BoolType});
  PARAM_PARAMVAR(_enabled, "Enabled", "Can be used to toggle this node when it has static persistence",
                 {shards::CoreInfo::BoolType, shards::CoreInfo::BoolVarType});
  PARAM_PARAMVAR(_shape, "Shape", "The shape of the body", {SHShape::VarType});
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
  PARAM_PARAMVAR(
      _collisionGroup, "CollisionGroup",
      "Collision filtering type (the first component contains group membership mask, the second part contains a filter mask)"
      "If any bits match the filter of the other, the two objects will collide",
      {CoreInfo::Int2Type, CoreInfo::Int2VarType});
  PARAM_VAR(_sensor, "Sensor", "Sensors only detect collisions but do not interact with collided objects (AKA triggers)",
            {CoreInfo::BoolType});
  PARAM_PARAMVAR(_tag, "Tag", "Tag for the body used in collision events", {CoreInfo::AnyType});
  PARAM_PARAMVAR(_context, "Context", "The physics context", {ShardsContext::VarType});
  PARAM_IMPL(PARAM_IMPL_FOR(_location), PARAM_IMPL_FOR(_rotation), PARAM_IMPL_FOR(_static), PARAM_IMPL_FOR(_enabled),
             PARAM_IMPL_FOR(_shape), PARAM_IMPL_FOR(_friction), PARAM_IMPL_FOR(_restitution), PARAM_IMPL_FOR(_linearDamping),
             PARAM_IMPL_FOR(_angularDamping), PARAM_IMPL_FOR(_maxLinearVelocity), PARAM_IMPL_FOR(_maxAngularVelocity),
             PARAM_IMPL_FOR(_gravityFactor), PARAM_IMPL_FOR(_allowedDOFs), PARAM_IMPL_FOR(_motionType),
             PARAM_IMPL_FOR(_collisionGroup),
             PARAM_IMPL_FOR(_sensor), //
             PARAM_IMPL_FOR(_tag), PARAM_IMPL_FOR(_context));

  ParamTypeMask _dynamicParameterMask = PTM_None;

  struct Instance {
    OwnedVar var;
    SHBody *node;

    Instance() {
      auto [node, var] = SHBody::ObjectVar.NewOwnedVar();
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
    _sensor = Var(false);
    _collisionGroup = toVar(int2(0xFFFFFFFF, 0xFFFFFFFF));
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
    if (_collisionGroup.isVariable())
      _dynamicParameterMask |= PTM_CollisionGroup;

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
    node->tag = _tag.get();

    node->params.friction = _friction.get().payload.floatValue;
    node->params.resitution = _restitution.get().payload.floatValue;
    node->params.linearDamping = _linearDamping.get().payload.floatValue;
    node->params.angularDamping = _angularDamping.get().payload.floatValue;

    node->params.maxLinearVelocity = _maxLinearVelocity.get().payload.floatValue;
    node->params.maxAngularVelocity = _maxAngularVelocity.get().payload.floatValue;
    node->params.gravityFactor = _gravityFactor.get().payload.floatValue;
    node->params.allowedDofs = convertAllowedDOFs(_allowedDOFs.get());
    node->params.motionType = (JPH::EMotionType)_motionType.get().payload.enumValue;

    auto cm = toUInt2(_collisionGroup.get());
    node->params.groupMembership = cm.x;
    node->params.collisionMask = cm.y;

    node->params.sensor = _sensor->payload.boolValue;

    node->location = toJPHVec3(_location.get().payload.float3Value);
    node->rotation = toJPHQuat(_rotation.get().payload.float4Value);
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
          inst.node->core = _requiredContext->core;
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
    if ((_dynamicParameterMask & 0b1111110000) != 0) {
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
      if (_dynamicParameterMask & PTM_CollisionGroup) {
        auto cm = toUInt2(_collisionGroup.get());
        innerNode->params.groupMembership = cm.x;
        innerNode->params.collisionMask = cm.y;
      }
      innerNode->updateParamHash1();
    }

    auto &shardsShape = varAsObjectChecked<SHShape>(_shape.get(), SHShape::Type);
    innerNode->shape = shardsShape.shape;
    innerNode->shapeUid = shardsShape.uid;

    assignVariableValue(_location.get(), toVar(innerNode->location));
    assignVariableValue(_rotation.get(), toVar(innerNode->rotation));

    return manyItem.var;
  }
};

struct CollisionsShard {
  static inline OwnedVar tk_otherId = OwnedVar::Foreign("otherId");
  static inline OwnedVar tk_otherTag = OwnedVar::Foreign("otherTag");
  static inline OwnedVar tk_penDepth = OwnedVar::Foreign("penetrationDepth");
  static inline OwnedVar tk_normal = OwnedVar::Foreign("normal");

  static inline shards::Types ContactTableTypes{CoreInfo::IntType, CoreInfo::AnyType, CoreInfo::FloatType, CoreInfo::Float3Type};
  static inline std::array<SHVar, 4> ContactTableKeys{
      Var("otherId"),
      Var("otherTag"),
      Var("penetrationDepth"),
      Var("normal"),
  };
  static inline Type ContactTableType = Type::TableOf(ContactTableTypes, ContactTableKeys);
  static inline Type ContactTableSeqType = Type::SeqOf(ContactTableType);

  static inline shards::Types ContactLeaveTableTypes{CoreInfo::IntType, CoreInfo::AnyType, CoreInfo::FloatType,
                                                     CoreInfo::Float3Type};
  static inline std::array<SHVar, 4> ContactLeaveTableKeys{
      Var("otherId"),
      Var("otherTag"),
  };
  static inline Type ContactLeaveTableType = Type::TableOf(ContactTableTypes, ContactTableKeys);

  static SHTypesInfo inputTypes() { return SHBody::Type; }
  static SHTypesInfo outputTypes() { return ContactTableSeqType; }
  static SHOptionalString help() { return SHCCSTR("Outputs the list of contacts of a physics body"); }

  PARAM(ShardsVar, _enter, "Enter", "Triggered when a new contact is removed", {shards::CoreInfo::ShardSeqOrNone});
  PARAM(ShardsVar, _leave_, "Leave", "Triggered when a new contact is removed", {shards::CoreInfo::ShardSeqOrNone});
  PARAM_IMPL(PARAM_IMPL_FOR(_enter), PARAM_IMPL_FOR(_leave_));

  struct PeristentBodyCollision {
    JPH::BodyID bodyId;
    std::shared_ptr<Node> node;
    uint64_t lastTouched{};
  };

  std::vector<PeristentBodyCollision> _uniqueCollisions;
  SeqVar _outputBuffer;
  TableVar _leaveTable;

  void warmup(SHContext *context) { PARAM_WARMUP(context); }
  void cleanup(SHContext *context) { PARAM_CLEANUP(context); }

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);

    SHInstanceData innerData{data};
    innerData.inputType = ContactTableType;
    _enter.compose(innerData);
    innerData.inputType = ContactLeaveTableType;
    _leave_.compose(innerData);

    return outputTypes().elements[0];
  }

  SHVar activate(SHContext *shContext, const SHVar &input) {
    _outputBuffer.clear();

    SHBody &shBody = varAsObjectChecked<SHBody>(input, SHBody::Type);
    shBody.node->neededCollisionEvents++;

    uint64_t frameCounter = shBody.core->frameCounter();
    auto &associatedData = shBody.node->data;
    if (associatedData && associatedData->body) {
      if (associatedData->events) {
        for (auto &event : (*associatedData->events)) {
          const JPH::Body *otherBody{};
          int selfIndex = 0;
          if (event->body0 == associatedData->body) {
            otherBody = event->body1;
            selfIndex = 0;
          } else {
            otherBody = event->body0;
            selfIndex = 1;
          }

          if (AssociatedData *otherData = shBody.core->getAssociatedData(otherBody)) {
            auto otherTag = otherData->node->tag;

            TableVar &outTable = _outputBuffer.emplace_back_table();
            outTable[tk_otherId] = Var(int64_t(otherData->body->GetID().GetIndex()));
            outTable[tk_otherTag] = OwnedVar::Foreign(otherTag);
            outTable[tk_penDepth] = Var(event->manifold.mPenetrationDepth);
            outTable[tk_normal] = toVar(selfIndex == 0 ? event->manifold.mWorldSpaceNormal : -event->manifold.mWorldSpaceNormal);

            auto it =
                std::find_if(_uniqueCollisions.begin(), _uniqueCollisions.end(), [&](const PeristentBodyCollision &collision) {
                  return collision.bodyId == otherData->body->GetID();
                });
            if (it == _uniqueCollisions.end()) {
              it = _uniqueCollisions.emplace(_uniqueCollisions.end(),
                                             PeristentBodyCollision{otherData->body->GetID(), otherData->node});
              SHVar unused{};
              _enter.activate(shContext, outTable, unused);
            }
            it->lastTouched = frameCounter;
          }
        }
      }
    }

    // Remove old collisions
    for (auto it = _uniqueCollisions.begin(); it != _uniqueCollisions.end();) {
      if (frameCounter - it->lastTouched > 1) {
        _leaveTable[tk_otherId] = Var(int64_t(it->bodyId.GetIndex()));
        _leaveTable[tk_otherTag] = it->node->tag;
        SHVar unused{};
        _leave_.activate(shContext, _leaveTable, unused);
        it = _uniqueCollisions.erase(it);
      } else {
        ++it;
      }
    }

    return _outputBuffer;
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

struct LinearVelocityShard {
  static SHTypesInfo inputTypes() { return SHBody::Type; }
  static SHTypesInfo outputTypes() { return shards::CoreInfo::Float3Type; }
  static SHOptionalString help() { return SHCCSTR("Retrieves the linear velocity of the physics body"); }

  PARAM_IMPL();
  void warmup(SHContext *context) { PARAM_WARMUP(context); }
  void cleanup(SHContext *context) { PARAM_CLEANUP(context); }
  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    return outputTypes().elements[0];
  }
  SHVar activate(SHContext *shContext, const SHVar &input) {
    SHBody &shBody = varAsObjectChecked<SHBody>(input, SHBody::Type);
    auto &associatedData = shBody.node->data;
    if (associatedData && associatedData->body) {
      auto &body = associatedData->body;
      auto &bodyInterface = shBody.core->getPhysicsSystem().GetBodyInterface();

      return toVar(bodyInterface.GetLinearVelocity(body->GetID()));
    } else {
      return shards::toVar(float3(0.0f));
    }
  }
};

struct AngularVelocityShard {
  static SHTypesInfo inputTypes() { return SHBody::Type; }
  static SHTypesInfo outputTypes() { return shards::CoreInfo::Float3Type; }
  static SHOptionalString help() { return SHCCSTR("Retrieves the angular velocity of the physics body"); }

  PARAM_IMPL();
  void warmup(SHContext *context) { PARAM_WARMUP(context); }
  void cleanup(SHContext *context) { PARAM_CLEANUP(context); }
  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    return outputTypes().elements[0];
  }
  SHVar activate(SHContext *shContext, const SHVar &input) {
    SHBody &shBody = varAsObjectChecked<SHBody>(input, SHBody::Type);
    auto &associatedData = shBody.node->data;
    if (associatedData && associatedData->body) {
      auto &body = associatedData->body;
      auto &bodyInterface = shBody.core->getPhysicsSystem().GetBodyInterface();

      return toVar(bodyInterface.GetAngularVelocity(body->GetID()));
    } else {
      return shards::toVar(float3(0.0f));
    }
  }
};

struct InverseMassShard {
  static SHTypesInfo inputTypes() { return SHBody::Type; }
  static SHTypesInfo outputTypes() { return shards::CoreInfo::FloatType; }
  static SHOptionalString help() { return SHCCSTR("Retrieves 1.0 / mass of the physics body"); }

  PARAM_IMPL();
  void warmup(SHContext *context) { PARAM_WARMUP(context); }
  void cleanup(SHContext *context) { PARAM_CLEANUP(context); }
  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    return outputTypes().elements[0];
  }
  SHVar activate(SHContext *shContext, const SHVar &input) {
    SHBody &shBody = varAsObjectChecked<SHBody>(input, SHBody::Type);
    auto &associatedData = shBody.node->data;
    if (associatedData && associatedData->body) {
      auto &body = associatedData->body;
      return Var(body->GetMotionProperties()->GetInverseMass());
    } else {
      return Var(0.0f);
    }
  }
};

struct CenterOfMassShard {
  static SHTypesInfo inputTypes() { return SHBody::Type; }
  static SHTypesInfo outputTypes() { return shards::CoreInfo::Float3Type; }
  static SHOptionalString help() { return SHCCSTR("Retrieves the center of mass of the physics body"); }

  PARAM_IMPL();
  void warmup(SHContext *context) { PARAM_WARMUP(context); }
  void cleanup(SHContext *context) { PARAM_CLEANUP(context); }
  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    return outputTypes().elements[0];
  }
  SHVar activate(SHContext *shContext, const SHVar &input) {
    SHBody &shBody = varAsObjectChecked<SHBody>(input, SHBody::Type);
    auto &associatedData = shBody.node->data;
    if (associatedData && associatedData->body) {
      auto &body = associatedData->body;
      auto &bodyInterface = shBody.core->getPhysicsSystem().GetBodyInterface();

      return toVar(bodyInterface.GetCenterOfMassPosition(body->GetID()));
    } else {
      return shards::toVar(float3(0.0f));
    }
  }
};

template <int Mode> struct ApplyVelocity {
  static_assert(Mode == 0 || Mode == 1, "Invalid mode");

  static SHTypesInfo inputTypes() { return SHBody::Type; }
  static SHTypesInfo outputTypes() { return SHBody::Type; }
  static SHOptionalString help() { return SHCCSTR("Applies a force to the physics body"); }

  PARAM_PARAMVAR(_linearForce, "Linear", "The linear force to apply", {shards::CoreInfo::Float3Type});
  PARAM_PARAMVAR(_angularForce, "Angular", "The angular force to apply", {shards::CoreInfo::Float3Type});
  PARAM_IMPL(PARAM_IMPL_FOR(_linearForce), PARAM_IMPL_FOR(_angularForce));

  ApplyVelocity() {
    _linearForce = shards::toVar(float3(0.0f));
    _angularForce = shards::toVar(float3(0.0f));
  }

  void warmup(SHContext *context) { PARAM_WARMUP(context); }
  void cleanup(SHContext *context) { PARAM_CLEANUP(context); }
  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    return outputTypes().elements[0];
  }
  SHVar activate(SHContext *shContext, const SHVar &input) {
    SHBody &shBody = varAsObjectChecked<SHBody>(input, SHBody::Type);
    auto &associatedData = shBody.node->data;
    if (associatedData && associatedData->body) {
      auto &body = associatedData->body;
      auto &bodyInterface = shBody.core->getPhysicsSystem().GetBodyInterface();

      if constexpr (Mode == 0) {
        bodyInterface.AddLinearAndAngularVelocity(body->GetID(), toJPHVec3(_linearForce.get().payload.float3Value),
                                                  toJPHVec3(_angularForce.get().payload.float3Value));
      } else {
        bodyInterface.AddForceAndTorque(body->GetID(), toJPHVec3(_linearForce.get().payload.float3Value),
                                        toJPHVec3(_angularForce.get().payload.float3Value));
      }
    }

    return input;
  }
};

struct ApplyForceAt {
  static SHTypesInfo inputTypes() { return SHBody::Type; }
  static SHTypesInfo outputTypes() { return SHBody::Type; }
  static SHOptionalString help() { return SHCCSTR("Applies a force to the physics body at a specific location"); }

  PARAM_PARAMVAR(_force, "Force", "The force to apply", {shards::CoreInfo::Float3Type});
  PARAM_PARAMVAR(_position, "Position", "The position to apply the force at", {shards::CoreInfo::Float3Type});
  PARAM_IMPL(PARAM_IMPL_FOR(_force), PARAM_IMPL_FOR(_position));

  ApplyForceAt() {
    _force = shards::toVar(float3(0.0f));
    _position = shards::toVar(float3(0.0f));
  }

  void warmup(SHContext *context) { PARAM_WARMUP(context); }
  void cleanup(SHContext *context) { PARAM_CLEANUP(context); }
  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    return outputTypes().elements[0];
  }
  SHVar activate(SHContext *shContext, const SHVar &input) {
    SHBody &shBody = varAsObjectChecked<SHBody>(input, SHBody::Type);
    auto &associatedData = shBody.node->data;
    if (associatedData && associatedData->body) {
      auto &body = associatedData->body;
      auto &bodyInterface = shBody.core->getPhysicsSystem().GetBodyInterface();

      auto at = toJPHVec3(_position.get().payload.float3Value);

      auto fpl = toJPHVec3(_force.get().payload.float3Value);
      if (fpl[0] != 0.0 && fpl[1] != 0.0 && fpl[2] != 0.0) {
        bodyInterface.AddForce(body->GetID(), fpl, at);
      }

      auto tpl = _force.get().payload.float3Value;
      if (tpl[0] != 0.0 && tpl[1] != 0.0 && tpl[2] != 0.0) {
        bodyInterface.AddTorque(body->GetID(), toJPHVec3(tpl));
      }
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
  REGISTER_SHARD("Physics.Collisions", CollisionsShard);
  REGISTER_SHARD("Physics.Dump", DumpShard);

  REGISTER_SHARD("Physics.LinearVelocity", LinearVelocityShard);
  REGISTER_SHARD("Physics.AngularVelocity", AngularVelocityShard);
  REGISTER_SHARD("Physics.InverseMass", InverseMassShard);
  REGISTER_SHARD("Physics.CenterOfMass", CenterOfMassShard);

  using ApplyImpulse = ApplyVelocity<0>;
  using ApplyForce = ApplyVelocity<1>;
  REGISTER_SHARD("Physics.ApplyImpulse", ApplyImpulse);
  REGISTER_SHARD("Physics.ApplyForce", ApplyForce);
  REGISTER_SHARD("Physics.ApplyForceAt", ApplyForceAt);
}