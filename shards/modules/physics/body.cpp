#include <shards/core/shared.hpp>
#include <shards/core/params.hpp>
#include <shards/core/object_var_util.hpp>
#include <shards/core/exposed_type_utils.hpp>
#include <shards/common_types.hpp>
#include <shards/linalg_shim.hpp>
#include <shards/gfx/types.hpp>
#include <gfx/hasherxxh3.hpp>
#include <gfx/drawable.hpp>
#include <tracy/Wrapper.hpp>
#include "physics.hpp"
#include "core.hpp"

namespace shards::Physics {
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
    SHBody *node{};

    Instance() {}
    Instance(Instance &&other) = default;
    Instance &operator=(Instance &&other) = default;
    void warmup() {
      auto [node, var] = SHBody::ObjectVar.NewOwnedVar();
      this->var = std::move(var);
      this->node = &node;
    }
    void cleanup() {
      var.reset();
      node = nullptr;
    }
    SHBody *operator->() { return node; }
  };
  Instance _instance;
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
    _instance.warmup();
  }

  void cleanup(SHContext *context) {
    if (_instance.node) {
      auto &bodyNode = _instance->node;
      bodyNode->enabled = false;
      bodyNode->persistence = false;
      bodyNode->selfVar.reset();
    }
    _instance.cleanup();
    _requiredContext.cleanup(context);
    PARAM_CLEANUP(context);
  }

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    _requiredContext.compose(data, _requiredVariables);

    if (!_location.isVariable())
      throw std::runtime_error("Location must be a variable");
    auto lv = findExposedVariable(data.shared, _location);
    if (!lv || !lv->isMutable)
      throw std::runtime_error("Location must be mutable");

    if (!_rotation.isVariable())
      throw std::runtime_error("Rotation must be a variable");
    auto rv = findExposedVariable(data.shared, _rotation);
    if (!rv || !rv->isMutable)
      throw std::runtime_error("Rotation must be mutable");

    if (!_shape.isVariable())
      throw std::runtime_error("Shape must be set");

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

  void initNewNode(std::shared_ptr<BodyNode> node) {
    if ((bool)*_static)
      node->persistence = true;
    node->enabled = _enabled.get().payload.boolValue;
    node->tag = _tag.get();

    auto &outParams = node->params.regular;
    outParams.friction = _friction.get().payload.floatValue;
    outParams.restitution = _restitution.get().payload.floatValue;
    outParams.linearDamping = _linearDamping.get().payload.floatValue;
    outParams.angularDamping = _angularDamping.get().payload.floatValue;

    outParams.maxLinearVelocity = _maxLinearVelocity.get().payload.floatValue;
    outParams.maxAngularVelocity = _maxAngularVelocity.get().payload.floatValue;
    outParams.gravityFactor = _gravityFactor.get().payload.floatValue;
    outParams.allowedDofs = convertAllowedDOFs(_allowedDOFs.get());
    outParams.motionType = (JPH::EMotionType)_motionType.get().payload.enumValue;

    auto cm = toUInt2(_collisionGroup.get());
    outParams.groupMembership = cm.x;
    outParams.collisionMask = cm.y;

    outParams.sensor = _sensor->payload.boolValue;

    node->location = toJPHVec3(_location.get().payload.float3Value);
    node->rotation = toJPHQuat(_rotation.get().payload.float4Value);
    node->updateParamHash0();
    node->updateParamHash1();
  }

  SHVar activate(SHContext *shContext, const SHVar &input) {
    ZoneScopedN("Physics::entity");

    if (!_instance->node) {
      _instance->node = _requiredContext->core->createNewBodyNode();
      _instance->node->selfVar = _instance.var;
      _instance->core = _requiredContext->core;
      initNewNode(_instance->node);
    }

    auto &instNode = _instance->node;

    instNode->enabled = _enabled.get().payload.boolValue;
    _requiredContext->core->touchNode(instNode);

    auto &outParams = instNode->params.regular;
    // Filtering to avoid unnecessary checks for static parameters
    if ((_dynamicParameterMask & 0b1111) != 0) {
      if (_dynamicParameterMask & PTM_Friction) {
        outParams.friction = _friction.get().payload.floatValue;
      }
      if (_dynamicParameterMask & PTM_Restitution) {
        outParams.restitution = _restitution.get().payload.floatValue;
      }
      if (_dynamicParameterMask & PTM_LinearDamping) {
        outParams.linearDamping = _linearDamping.get().payload.floatValue;
      }
      if (_dynamicParameterMask & PTM_AngularDamping) {
        outParams.angularDamping = _angularDamping.get().payload.floatValue;
      }
      instNode->updateParamHash0();
    }
    if ((_dynamicParameterMask & 0b1111110000) != 0) {
      if (_dynamicParameterMask & PTM_MaxLinearVelocity) {
        outParams.maxLinearVelocity = _maxLinearVelocity.get().payload.floatValue;
      }
      if (_dynamicParameterMask & PTM_MaxAngularVelocity) {
        outParams.maxAngularVelocity = _maxAngularVelocity.get().payload.floatValue;
      }
      if (_dynamicParameterMask & PTM_GravityFactor) {
        outParams.gravityFactor = _gravityFactor.get().payload.floatValue;
      }
      if (_dynamicParameterMask & PTM_AllowedDOFs) {
        outParams.allowedDofs = convertAllowedDOFs(_allowedDOFs.get());
      }
      if (_dynamicParameterMask & PTM_MotionType) {
        outParams.motionType = (JPH::EMotionType)_motionType.get().payload.enumValue;
      }
      if (_dynamicParameterMask & PTM_CollisionGroup) {
        auto cm = toUInt2(_collisionGroup.get());
        outParams.groupMembership = cm.x;
        outParams.collisionMask = cm.y;
      }
      instNode->updateParamHash1();
    }

    auto &shardsShape = varAsObjectChecked<SHShape>(_shape.get(), SHShape::Type);
    instNode->shape = shardsShape.shape;
    instNode->shapeUid = shardsShape.uid;

    assignVariableValue(_location.get(), toVar(instNode->location));
    assignVariableValue(_rotation.get(), toVar(instNode->rotation));

    return _instance.var;
  }
};
} // namespace shards::Physics

SHARDS_REGISTER_FN(body) {
  using namespace shards::Physics;
  REGISTER_SHARD("Physics.Body", BodyShard);
}