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
  PTM_Mass = 1 << 10,
  // Ensure PTM_Mass is handled in all relevant parts of the code
};

static ParamTypeMask &operator|=(ParamTypeMask &lhs, ParamTypeMask rhs) {
  return lhs = static_cast<ParamTypeMask>(static_cast<uint32_t>(lhs) | static_cast<uint32_t>(rhs));
}

struct BodyShard {
  static SHTypesInfo inputTypes() { return shards::CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return SHBody::Type; }

  static SHOptionalString inputHelp() { return DefaultHelpText::InputHelpIgnored; }
  static SHOptionalString help() {
    return SHCCSTR(
        "This shard creates a physics body, conducts physics simulations on this body while updating the relavent variables tied "
        "to this body, and creates an interface to allow other physics shards to interact with this body.");
  }
  static SHOptionalString outputHelp() {
    return SHCCSTR("Outputs the physics body created by this shard as a physics object that acts as an interface for other "
                   "physics shard to interact with the body.");
  }

  static inline Type PhysicsDOFSeqType = Type::SeqOf(PhysicsDOFEnumInfo::Type);
  static inline Types PhysicsDOFTypes{PhysicsDOFEnumInfo::Type, PhysicsDOFSeqType, Type::VariableOf(PhysicsDOFSeqType)};

  PARAM_PARAMVAR(_location, "Location", "The initial location of the physics object. The variable provided in this parameter is also updated through the physics simulations conducted on this body. Vice versa, the body's location is also updated if the variable's value is changed.", {shards::CoreInfo::Float3VarType});
  PARAM_PARAMVAR(_rotation, "Rotation", "The initial rotation of the physics object. The variable provided in this parameter is also updated through the physics simulations conducted on this body. Vice versa, the body's rotation is also updated if the variable's value is changed.", {shards::CoreInfo::Float4VarType});
  PARAM_VAR(_static, "Static", "If false, the physics body will be destroyed when the shard is not activated. If true, the body will persist and be included in the physics simulation even if the shard is not activated.", {shards::CoreInfo::BoolType});
  PARAM_PARAMVAR(_enabled, "Enabled", "Can be used to toggle the body on or off if it is a persistent body. If false, the body is temporarily removed from the simulation without destroying it.",
                 {shards::CoreInfo::BoolType, shards::CoreInfo::BoolVarType});
  PARAM_PARAMVAR(_shape, "Shape", "The shape of the body.", {SHShape::VarType});
  PARAM_PARAMVAR(_friction, "Friction", "The friction applied when this physics body is in contact with another physics body.", {shards::CoreInfo::FloatType, shards::CoreInfo::FloatVarType});
  PARAM_PARAMVAR(_restitution, "Restitution", "The bounciness of the body when it collides with another physics body.",
                 {shards::CoreInfo::FloatType, shards::CoreInfo::FloatVarType});
  PARAM_PARAMVAR(_linearDamping, "LinearDamping", "How much linear velocity decays over time.",
                 {shards::CoreInfo::FloatType, shards::CoreInfo::FloatVarType});
  PARAM_PARAMVAR(_angularDamping, "AngularDamping", "How much angular velocity decays over time.",
                 {shards::CoreInfo::FloatType, shards::CoreInfo::FloatVarType});
  PARAM_PARAMVAR(_maxLinearVelocity, "MaxLinearVelocity", "Max linear velocity",
                 {shards::CoreInfo::FloatType, shards::CoreInfo::FloatVarType});
  PARAM_PARAMVAR(_maxAngularVelocity, "MaxAngularVelocity", "Max angular velocity",
                 {shards::CoreInfo::FloatType, shards::CoreInfo::FloatVarType});
  PARAM_PARAMVAR(_gravityFactor, "GravityFactor", "The gravity factor applied to this body",
                 {shards::CoreInfo::FloatType, shards::CoreInfo::FloatVarType});
  PARAM_PARAMVAR(_allowedDOFs, "AllowedDOFs", "The translation and rotation axes that the body is allowed to move and rotate around.", PhysicsDOFTypes);
  PARAM_PARAMVAR(_motionType, "MotionType", "Motion type of the body, Dynamic, Kinematic, or Static.",
                 {PhysicsMotionEnumInfo::Type, Type::VariableOf(PhysicsMotionEnumInfo::Type)});
  PARAM_PARAMVAR(
      _collisionGroup, "CollisionGroup",
      "The collision group this body belongs to and which collision groups it is allowed to collide with. The first component in the int2 dictates collision group membership mask, the second part contains a filter mask.",
      {CoreInfo::Int2Type, CoreInfo::Int2VarType});
  PARAM_VAR(_sensor, "Sensor", "If true, this physics body will be considered a Sensor. Sensors only detect collisions but do not interact with collided objects (AKA triggers)",
            {CoreInfo::BoolType});
  PARAM_PARAMVAR(_mass, "Mass", "Mass of the body. For mass less or equal to 0, default mass calculation is used instead.",
                 {CoreInfo::FloatType, CoreInfo::FloatVarType});
  PARAM_PARAMVAR(_tag, "Tag", "Tag attached to this body for use in collision events.", {CoreInfo::AnyType});
  PARAM_PARAMVAR(_context, "Context", "The physics context object that is managing the physics simulation.", {ShardsContext::VarType});
  PARAM_IMPL(PARAM_IMPL_FOR(_location), PARAM_IMPL_FOR(_rotation), PARAM_IMPL_FOR(_static), PARAM_IMPL_FOR(_enabled),
             PARAM_IMPL_FOR(_shape), PARAM_IMPL_FOR(_friction), PARAM_IMPL_FOR(_restitution), PARAM_IMPL_FOR(_linearDamping),
             PARAM_IMPL_FOR(_angularDamping), PARAM_IMPL_FOR(_maxLinearVelocity), PARAM_IMPL_FOR(_maxAngularVelocity),
             PARAM_IMPL_FOR(_gravityFactor), PARAM_IMPL_FOR(_allowedDOFs), PARAM_IMPL_FOR(_motionType),
             PARAM_IMPL_FOR(_collisionGroup),
             PARAM_IMPL_FOR(_sensor), //
             PARAM_IMPL_FOR(_mass), PARAM_IMPL_FOR(_tag), PARAM_IMPL_FOR(_context));

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
    _mass = Var(0.0f);
  }

  void warmup(SHContext *context) {
    PARAM_WARMUP(context);
    _requiredContext.warmup(context, &_context);
    _instance.warmup();
  }

  void cleanup(SHContext *context) {
    if (_instance.node && _instance.node->node) {
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
    if (_mass.isVariable())
      _dynamicParameterMask |= PTM_Mass;

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

    outParams.mass = _mass.get().payload.floatValue;

    auto cm = toUInt2(_collisionGroup.get());
    outParams.groupMembership = cm.x;
    outParams.collisionMask = cm.y;

    outParams.sensor = _sensor->payload.boolValue;

    memcpy(&node->location.payload, &_location.get().payload, sizeof(SHVarPayload));
    memcpy(&node->rotation.payload, &_rotation.get().payload, sizeof(SHVarPayload));
    node->prevLocation = node->location;
    node->prevRotation = node->rotation;
    node->updateParamHash0();
    node->updateParamHash1();

    auto &shardsShape = varAsObjectChecked<SHShape>(_shape.get(), SHShape::Type);
    node->shape = shardsShape.shape;
    node->shapeUid = shardsShape.uid;

    _requiredContext->core->touchNode(_instance->node);
    _requiredContext->core->initializeBodyImmediate(_instance->node);
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

    if ((_dynamicParameterMask & 0b11111110000) != 0) {
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
      if (_dynamicParameterMask & PTM_Mass) {
        outParams.mass = _mass.get().payload.floatValue;
      }
      instNode->updateParamHash1();
    }

    auto &shardsShape = varAsObjectChecked<SHShape>(_shape.get(), SHShape::Type);
    instNode->shape = shardsShape.shape;
    instNode->shapeUid = shardsShape.uid;

    bool updatePose{};
    if (instNode->data->getPhysicsObject()) {
      if (memcmp(&instNode->prevLocation.payload, &_location.get().payload, sizeof(SHFloat3)) != 0) {
        memcpy(&instNode->location.payload, &_location.get().payload, sizeof(SHVarPayload));
        updatePose = true;
      }
      if (memcmp(&instNode->prevRotation.payload, &_rotation.get().payload, sizeof(SHFloat4)) != 0) {
        memcpy(&instNode->rotation.payload, &_rotation.get().payload, sizeof(SHVarPayload));
        updatePose = true;
      }
      if (updatePose) {
        auto &bi = _requiredContext->core->getPhysicsSystem().GetBodyInterface();
        bi.SetPositionAndRotation(instNode->data->body->GetID(), toJPHVec3(instNode->location), toJPHQuat(instNode->rotation),
                                  JPH::EActivation::Activate);
      }
    }

    if (!updatePose) {
      assignVariableValue(_location.get(), toVar(instNode->location));
      assignVariableValue(_rotation.get(), toVar(instNode->rotation));
    }

    return _instance.var;
  }
};
} // namespace shards::Physics

SHARDS_REGISTER_FN(body) {
  using namespace shards::Physics;
  REGISTER_SHARD("Physics.Body", BodyShard);
}