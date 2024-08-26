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

#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>

namespace shards::Physics {
std::atomic_uint64_t BodyNode::UidCounter = 0;
std::atomic_uint64_t SHShape::UidCounter;
std::atomic_uint64_t SHSoftBodyShape::UidCounter;

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

  // Upper limit for the MaxIterations parameter
  static inline const int MaxIterationsUpperBound = 128;

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

    if (numIterations < 1 || numIterations > MaxIterationsUpperBound)
      throw std::runtime_error(fmt::format("MaxIterations must be at least 1 and at most {}", MaxIterationsUpperBound));
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

struct CollisionsShard {
  static inline OwnedVar tk_other = OwnedVar::Foreign("other");
  static inline OwnedVar tk_otherTag = OwnedVar::Foreign("otherTag");
  static inline OwnedVar tk_penDepth = OwnedVar::Foreign("penetrationDepth");
  static inline OwnedVar tk_normal = OwnedVar::Foreign("normal");

  static inline shards::Types ContactTableTypes{SHBody::Type, CoreInfo::AnyType, CoreInfo::FloatType, CoreInfo::Float3Type};
  static inline std::array<SHVar, 4> ContactTableKeys{
      tk_other,
      tk_otherTag,
      tk_penDepth,
      tk_normal,
  };
  static inline Type ContactTableType = Type::TableOf(ContactTableTypes, ContactTableKeys);
  static inline Type ContactTableSeqType = Type::SeqOf(ContactTableType);

  static inline shards::Types ContactLeaveTableTypes{SHBody::Type, CoreInfo::AnyType};
  static inline std::array<SHVar, 2> ContactLeaveTableKeys{
      tk_other,
      tk_otherTag,
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
    std::shared_ptr<BodyNode> node;
    uint64_t lastTouched{};
  };

  std::vector<PeristentBodyCollision> _uniqueCollisions;
  std::vector<SHBody> tempBodies;
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

          if (BodyAssociatedData *otherData = shBody.core->findBodyAssociatedData(otherBody)) {
            auto otherTag = otherData->node->tag;

            TableVar &outTable = _outputBuffer.emplace_back_table();

            outTable[tk_other] = otherData->node->selfVar;
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
        _leaveTable[tk_other] = it->node->selfVar;
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
    SPDLOG_INFO("Physics Core:");
    const auto &nodeMap = core->getBodyNodeMap();
    SPDLOG_INFO(" Nodes: {}", nodeMap.size());
    for (auto &node : nodeMap) {
      SPDLOG_INFO("  BodyNode: {}({}) LT: {}", node.first, (void *)node.second.node.get(), node.second.lastTouched);
    }
    SPDLOG_INFO(" Active Nodes: {}", core->getActiveBodyNodes().size());
    for (auto &node : core->getActiveBodyNodes()) {
      SPDLOG_INFO("  BodyNode: {}({})", node->node->uid, (void *)node->node.get());
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

struct MotionTypeShard {
  static SHTypesInfo inputTypes() { return SHBody::Type; }
  static SHTypesInfo outputTypes() { return PhysicsMotionEnumInfo::Type; }
  static SHOptionalString help() { return SHCCSTR("Retrieves the motion type of the physics body"); }

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
      return Var::Enum(body->GetMotionType(), PhysicsMotionEnumInfo::Type);
    } else {
      return Var::Enum(JPH::EMotionType::Static, PhysicsMotionEnumInfo::Type);
    }
  }
};

struct SetPoses {
  static SHTypesInfo inputTypes() { return SHBody::Type; }
  static SHTypesInfo outputTypes() { return SHBody::Type; }
  static SHOptionalString help() { return SHCCSTR("Override poses of the physics body"); }

  PARAM_PARAMVAR(_position, "Linear", "The position to set",
                 {shards::CoreInfo::NoneType, shards::CoreInfo::Float3Type, CoreInfo::Float3VarType});
  PARAM_PARAMVAR(_rotation, "Angular", "The rotation to set",
                 {shards::CoreInfo::NoneType, shards::CoreInfo::Float4Type, CoreInfo::Float4VarType});
  PARAM_IMPL(PARAM_IMPL_FOR(_position), PARAM_IMPL_FOR(_rotation));

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
    if (associatedData && associatedData->bodyAdded) {
      auto &body = associatedData->body;
      auto &bodyInterface = shBody.core->getPhysicsSystem().GetBodyInterface();

      if (_position.get().valueType == SHType::Float3) {
        bodyInterface.SetPosition(body->GetID(), toJPHVec3(_position.get().payload.float3Value), JPH::EActivation::Activate);
      }
      if (_rotation.get().valueType == SHType::Float4) {
        bodyInterface.SetRotation(body->GetID(), toJPHQuat(_rotation.get().payload.float4Value), JPH::EActivation::Activate);
      }
    }
    return input;
  }
};

struct SetVelocities {
  static SHTypesInfo inputTypes() { return SHBody::Type; }
  static SHTypesInfo outputTypes() { return SHBody::Type; }
  static SHOptionalString help() { return SHCCSTR("Override velocity of the physics body"); }

  PARAM_PARAMVAR(_linearVel, "Linear", "The linear velocity to set",
                 {shards::CoreInfo::NoneType, shards::CoreInfo::Float3Type, CoreInfo::Float3VarType});
  PARAM_PARAMVAR(_angularVel, "Angular", "The angular velocity to set",
                 {shards::CoreInfo::NoneType, shards::CoreInfo::Float3Type, CoreInfo::Float3VarType});
  PARAM_IMPL(PARAM_IMPL_FOR(_linearVel), PARAM_IMPL_FOR(_angularVel));

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
    if (associatedData && associatedData->bodyAdded) {
      auto &body = associatedData->body;
      auto &bodyInterface = shBody.core->getPhysicsSystem().GetBodyInterface();

      if (_linearVel.get().valueType == SHType::Float3) {
        bodyInterface.SetLinearVelocity(body->GetID(), toJPHVec3(_linearVel.get().payload.float3Value));
      }
      if (_angularVel.get().valueType == SHType::Float3) {
        bodyInterface.SetAngularVelocity(body->GetID(), toJPHVec3(_angularVel.get().payload.float3Value));
      }
    }
    return input;
  }
};

template <int Mode> struct ApplyVelocity {
  static_assert(Mode == 0 || Mode == 1, "Invalid mode");

  static SHTypesInfo inputTypes() { return SHBody::Type; }
  static SHTypesInfo outputTypes() { return SHBody::Type; }
  static SHOptionalString help() { return SHCCSTR("Applies a force to the physics body"); }

  PARAM_PARAMVAR(_linearForce, "Linear", "The linear force to apply", {shards::CoreInfo::Float3Type, CoreInfo::Float3VarType});
  PARAM_PARAMVAR(_angularForce, "Angular", "The angular force to apply", {shards::CoreInfo::Float3Type, CoreInfo::Float3VarType});
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
    if (associatedData && associatedData->bodyAdded) {
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

  PARAM_PARAMVAR(_force, "Force", "The force to apply", {shards::CoreInfo::Float3Type, CoreInfo::Float3VarType});
  PARAM_PARAMVAR(_position, "Position", "The position to apply the force at",
                 {shards::CoreInfo::Float3Type, CoreInfo::Float3VarType});
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
    if (associatedData && associatedData->bodyAdded) {
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
  REGISTER_SHARD("Physics.Collisions", CollisionsShard);
  REGISTER_SHARD("Physics.Dump", DumpShard);

  REGISTER_SHARD("Physics.LinearVelocity", LinearVelocityShard);
  REGISTER_SHARD("Physics.AngularVelocity", AngularVelocityShard);
  REGISTER_SHARD("Physics.InverseMass", InverseMassShard);
  REGISTER_SHARD("Physics.CenterOfMass", CenterOfMassShard);
  REGISTER_SHARD("Physics.MotionType", MotionTypeShard);

  REGISTER_SHARD("Physics.SetPose", SetPoses);
  REGISTER_SHARD("Physics.SetVelocity", SetVelocities);

  using ApplyImpulse = ApplyVelocity<0>;
  using ApplyForce = ApplyVelocity<1>;
  REGISTER_SHARD("Physics.ApplyImpulse", ApplyImpulse);
  REGISTER_SHARD("Physics.ApplyForce", ApplyForce);
  REGISTER_SHARD("Physics.ApplyForceAt", ApplyForceAt);
}