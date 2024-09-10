#include <shards/core/shared.hpp>
#include <shards/core/params.hpp>
#include <shards/common_types.hpp>
#include <shards/linalg_shim.hpp>
#include "constraints.hpp"
#include "core.hpp"
#include <Jolt/Physics/Constraints/FixedConstraint.h>
#include <Jolt/Physics/Constraints/DistanceConstraint.h>
#include <Jolt/Physics/Constraints/SliderConstraint.h>
#include <gfx/linalg.hpp>

namespace shards::Physics {

DECL_ENUM_INFO(JPH::EConstraintSpace, ConstraintSpace,
               "Defines the coordinate space for physics constraints. Affects how constraints are applied and calculated in the "
               "physics simulation.",
               'phCs');

std::atomic_uint64_t ConstraintNode::UidCounter{};

template <typename T>
concept ConstraintImpl = requires(T &t, const std::shared_ptr<ConstraintNode> &nd, const std::shared_ptr<Core> &core) {
  { t.createNode(core) } -> std::same_as<std::shared_ptr<ConstraintNode>>;
  { t.updateNodeParams(nd) };
};

struct ConstraintBase {
  static inline auto Float4x4VarType = Type::VariableOf(CoreInfo::Float4x4Type);
  static inline Types Float4x4OrVarTypes{CoreInfo::Float4x4Type, Float4x4VarType};
  static inline Mat4 Float4x4Identity{linalg::identity};

  PARAM_PARAMVAR(_bodyA, "FirstBody", "The first body, keep unset to attach to the fixed world",
                 {SHBody::VarType, CoreInfo::NoneType});
  PARAM_PARAMVAR(_bodyB, "SecondBody", "The second body, keep unset to attach to the fixed world",
                 {SHBody::VarType, CoreInfo::NoneType});
  PARAM_VAR(_static, "Static", "Static node, persist when not activated", {shards::CoreInfo::BoolType});
  PARAM_PARAMVAR(_enabled, "Enabled", "Can be used to toggle this node when it has static persistence",
                 {shards::CoreInfo::BoolType, shards::CoreInfo::BoolVarType});
  PARAM_IMPL(PARAM_IMPL_FOR(_bodyA), PARAM_IMPL_FOR(_bodyB), PARAM_IMPL_FOR(_static), PARAM_IMPL_FOR(_enabled));

  std::shared_ptr<ConstraintNode> _constraint;
  RequiredContext _context;

  ConstraintBase() {
    _static = Var(false);
    _enabled = Var(true);
  }

  void baseWarmup(SHContext *context) { _context.warmup(context); }
  void baseCleanup(SHContext *context) {
    if (_constraint) {
      _constraint->enabled = false;
      _constraint->persistence = false;
      _constraint.reset();
    }
    _context.cleanup(context);
  }
  void baseCompose(SHInstanceData &data, ExposedInfo &requiredVariables) {
    _context.compose(data, requiredVariables);

    if (_bodyA.isNone() && _bodyB.isNone()) {
      throw std::runtime_error("At least one body must be set");
    }
  }

  uint64_t getBodyOrId(const SHVar &bodyVar) {
    if (bodyVar.valueType == SHType::None) {
      return ~0;
    } else {
      auto &body = varAsObjectChecked<SHBody>(bodyVar, SHBody::Type);
      return body.node->uid;
    }
  }

  template <ConstraintImpl Impl> SHVar baseActivate(SHContext *shContext, const SHVar &input, Impl &impl) {
    auto bodyAUid = getBodyOrId(_bodyA.get());
    auto bodyBUid = getBodyOrId(_bodyB.get());

    if (!_constraint) {
      _constraint = impl.createNode(_context->core); // ->createNewConstraintNode<FixedConstraintParams>();
      if ((bool)*_static)
        _constraint->persistence = true;
    }

    _constraint->enabled = _enabled.get().payload.boolValue;

    bool changed{};
    auto &params = _constraint->getParamsAs<ConstraintParams>();
    if (params->bodyNodeIdA != bodyAUid) {
      params->bodyNodeIdA = bodyAUid;
      changed = true;
    }
    if (params->bodyNodeIdB != bodyBUid) {
      params->bodyNodeIdB = bodyBUid;
      changed = true;
    }

    // Fixed constraint is only updated when bodies change, stays fixed otherwise
    if (changed) {
      impl.updateNodeParams(_constraint);
    }

    _context->core->touchNode(_constraint);

    return input;
  }
};

// Constraint that doesn't update its parameters ever, only set on create
template <typename T = JPH::FixedConstraintSettings> struct SetOnCreateConstraintParams final : public ConstraintParams {
  JPH::Ref<T> settings;

  SetOnCreateConstraintParams() {
    constraintType = 0;
    settings = new T();
  }
  virtual JPH::TwoBodyConstraintSettings *getSettings() override { return settings.GetPtr(); }
  uint64_t updateParamHash0() override { return 0; }
};

struct FixedConstraint final : public ConstraintBase {
  static SHTypesInfo inputTypes() { return shards::CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return shards::CoreInfo::AnyType; }
  static SHOptionalString help() { return SHCCSTR(""); }

  PARAM_VAR(_space, "Space",
            "This determines in which space the constraint is setup, all other properties should be in the specified space",
            {ConstraintSpaceEnumInfo::Type});
  PARAM_PARAMVAR(_refA, "FirstReferenceFrame", "Body 1 constraint reference frame (space determined by Space)",
                 Float4x4OrVarTypes);
  PARAM_PARAMVAR(_refB, "SecondReferenceFrame", "Body 2 constraint reference frame (space determined by Space)",
                 Float4x4OrVarTypes);
  PARAM_PARAMVAR(_autoDetectPoint, "AutoDetectPoint",
                 "When Space is WorldSpace FirstReferenceFrame and SecondReferenceFrame can be automatically calculated based on "
                 "the positions of the "
                 "bodies when the constraint is created (they will be fixated in their current relative position/orientation). ",
                 {CoreInfo::BoolType, CoreInfo::BoolVarType});

  PARAM_IMPL_DERIVED(ConstraintBase, PARAM_IMPL_FOR(_space), PARAM_IMPL_FOR(_refA), PARAM_IMPL_FOR(_refB),
                     PARAM_IMPL_FOR(_autoDetectPoint));

  FixedConstraint() {
    _autoDetectPoint = Var(true);
    _space = Var::Enum(JPH::EConstraintSpace::WorldSpace, ConstraintSpaceEnumInfo::Type);
    _refA = Float4x4Identity;
    _refB = Float4x4Identity;
  }

  void warmup(SHContext *context) {
    PARAM_WARMUP(context);
    baseWarmup(context);
  }
  void cleanup(SHContext *context) {
    PARAM_CLEANUP(context);
    baseCleanup(context);
  }

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    baseCompose(data, _requiredVariables);
    return data.inputType;
  }

  SHVar activate(SHContext *shContext, const SHVar &input) { return baseActivate(shContext, input, *this); }

  using Params = SetOnCreateConstraintParams<JPH::FixedConstraintSettings>;
  std::shared_ptr<ConstraintNode> createNode(const std::shared_ptr<Core> &core) {
    return core->createNewConstraintNode<Params>();
  }
  void updateNodeParams(const std::shared_ptr<ConstraintNode> &nd) {
    auto &params = nd->getParamsAs<Params>();
    params->settings->mSpace = (JPH::EConstraintSpace)_space.payload.enumValue;
    params->settings->mAutoDetectPoint = _autoDetectPoint.get().payload.boolValue;
    auto &ref0 = Mat4::fromVar(_refA.get());
    params->settings->mPoint1 = toJPHVec3(ref0.w.payload.xyz());
    params->settings->mAxisX1 = toJPHVec3(ref0.x.payload.xyz());
    params->settings->mAxisY1 = toJPHVec3(ref0.y.payload.xyz());
    auto &ref1 = Mat4::fromVar(_refB.get());
    params->settings->mPoint2 = toJPHVec3(ref1.w.payload.xyz());
    params->settings->mAxisX2 = toJPHVec3(ref1.x.payload.xyz());
    params->settings->mAxisY2 = toJPHVec3(ref1.y.payload.xyz());
    params->updateParamHash0();
  }
};

struct SpringSettings {
  static inline std::array<SHVar, 2> Keys0{Var("frequency"), Var("damping")};
  static inline shards::Types Types0{
      CoreInfo::FloatType,
      CoreInfo::FloatType,
  };
  // Frequency + damping
  static inline Type Type0 = Type::TableOf(Types0, Keys0);
  static inline std::array<SHVar, 2> Keys1{Var("stiffness"), Var("damping")};
  static inline shards::Types Types1{
      CoreInfo::FloatType,
      CoreInfo::FloatType,
  };
  // Stiffness + damping
  static inline Type Type1 = Type::TableOf(Types1, Keys1);

  static inline shards::Types Types{Type0, Type1};
  static inline shards::Types TypesVarOrNone{CoreInfo::NoneType, Type0, Type1, Type::VariableOf(Type0), Type::VariableOf(Type1)};

  static inline TableVar Default = []() {
    TableVar tv;
    tv["frequency"] = Var(0.0f);
    tv["damping"] = Var(1.0f);
    return tv;
  }();

  JPH::SpringSettings from(const SHVar &var) {
    JPH::SpringSettings settings;
    if (var.valueType == SHType::Table) {
      auto &table = (TableVar &)var;
      static SHVar tk_freq = Var("frequency");
      static SHVar tk_stiff = Var("stiffness");
      static SHVar tk_damp = Var("damping");
      if (table.hasKey(tk_freq)) {
        settings.mMode = JPH::ESpringMode::FrequencyAndDamping;
        settings.mFrequency = table[tk_freq].payload.floatValue;
        settings.mDamping = table[tk_damp].payload.floatValue;
      } else {
        settings.mMode = JPH::ESpringMode::StiffnessAndDamping;
        settings.mStiffness = table[tk_stiff].payload.floatValue;
        settings.mDamping = table[tk_damp].payload.floatValue;
      }
    }
    return settings;
  }
};

struct MotorSettings {
  static inline std::array<SHVar, 4> Keys0{
      Var("minForceLimit"),  //
      Var("maxForceLimit"),  //
      Var("minTorqueLimit"), //
      Var("maxTorqueLimit"), //
  };
  static inline shards::Types Types0{
      CoreInfo::FloatType,
      CoreInfo::FloatType,
      CoreInfo::FloatType,
      CoreInfo::FloatType,
  };
  static inline Type Type0 = Type::TableOf(Types0, Keys0);

  static inline TableVar Default = []() {
    TableVar tv;
    tv["minForceLimit"] = Var(std::numeric_limits<float>::min());
    tv["maxForceLimit"] = Var(std::numeric_limits<float>::max());
    tv["minTorqueLimit"] = Var(std::numeric_limits<float>::min());
    tv["maxTorqueLimit"] = Var(std::numeric_limits<float>::max());
    return tv;
  }();

  static inline shards::Types TypesVarOrNone{CoreInfo::NoneType, Type0, Type::VariableOf(Type0)};

  JPH::MotorSettings from(const SHVar &var) {
    JPH::MotorSettings settings;
    if (var.valueType == SHType::Table) {
      auto &table = (TableVar &)var;
      settings.mMinForceLimit = table[Var("minForceLimit")].payload.floatValue;
      settings.mMaxForceLimit = table[Var("maxForceLimit")].payload.floatValue;
      settings.mMinTorqueLimit = table[Var("minTorqueLimit")].payload.floatValue;
      settings.mMaxTorqueLimit = table[Var("maxTorqueLimit")].payload.floatValue;
    }
    return settings;
  }
};

struct DistanceConstraint final : public ConstraintBase {
  static SHTypesInfo inputTypes() { return shards::CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return shards::CoreInfo::AnyType; }
  static SHOptionalString help() { return SHCCSTR(""); }

  PARAM_VAR(_space, "Space",
            "This determines in which space the constraint is setup, all other properties should be in the specified space",
            {ConstraintSpaceEnumInfo::Type});
  PARAM_PARAMVAR(_refA, "FirstPoint", "Body 1 constraint reference frame (space determined by Space)",
                 {CoreInfo::Float3Type, CoreInfo::Float3VarType});
  PARAM_PARAMVAR(_refB, "SecondPoint", "Body 2 constraint reference frame (space determined by Space)",
                 {CoreInfo::Float3Type, CoreInfo::Float3VarType});

  PARAM_PARAMVAR(_minDistance, "MinDistance",
                 "Minimum distance between the two points. If the value is negative, it will be replaced by the distance between "
                 "FirstPoint and SecondPoint (works only if Space is world space)",
                 {CoreInfo::FloatType, CoreInfo::FloatVarType});
  PARAM_PARAMVAR(_maxDistance, "MaxDistance",
                 "Maximum distance between the two points. If the value is negative, it will be replaced by the distance between "
                 "FirstPoint and SecondPoint (works only if Space is world space)",
                 {CoreInfo::FloatType, CoreInfo::FloatVarType});

  PARAM_PARAMVAR(_limitSpring, "LimitSpring", "When set, makes the limits soft", SpringSettings::TypesVarOrNone);

  PARAM_IMPL_DERIVED(ConstraintBase, PARAM_IMPL_FOR(_space), PARAM_IMPL_FOR(_refA), PARAM_IMPL_FOR(_refB),
                     PARAM_IMPL_FOR(_minDistance), PARAM_IMPL_FOR(_maxDistance), PARAM_IMPL_FOR(_limitSpring));

  DistanceConstraint() {
    _space = Var::Enum(JPH::EConstraintSpace::LocalToBodyCOM, ConstraintSpaceEnumInfo::Type);
    _refA = toVar(float3(0.0f));
    _refB = toVar(float3(0.0f));
    _minDistance = Var(-1.0f);
    _maxDistance = Var(-1.0f);
    _limitSpring = SpringSettings::Default;
  }

  void warmup(SHContext *context) {
    PARAM_WARMUP(context);
    baseWarmup(context);
  }
  void cleanup(SHContext *context) {
    PARAM_CLEANUP(context);
    baseCleanup(context);
  }

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    baseCompose(data, _requiredVariables);
    return data.inputType;
  }

  SHVar activate(SHContext *shContext, const SHVar &input) { return baseActivate(shContext, input, *this); }

  using Params = SetOnCreateConstraintParams<JPH::DistanceConstraintSettings>;
  std::shared_ptr<ConstraintNode> createNode(const std::shared_ptr<Core> &core) {
    return core->createNewConstraintNode<Params>();
  }
  void updateNodeParams(const std::shared_ptr<ConstraintNode> &nd) {
    auto &params = nd->getParamsAs<Params>();
    params->settings->mSpace = (JPH::EConstraintSpace)_space.payload.enumValue;
    params->settings->mPoint1 = toJPHVec3(_refA.get().payload.float3Value);
    params->settings->mPoint2 = toJPHVec3(_refB.get().payload.float3Value);
    params->settings->mMinDistance = _minDistance.get().payload.floatValue;
    params->settings->mMaxDistance = _maxDistance.get().payload.floatValue;
    params->settings->mLimitsSpringSettings = SpringSettings().from(_limitSpring.get());
    params->updateParamHash0();
  }
};

struct SliderConstraint final : public ConstraintBase {
  static SHTypesInfo inputTypes() { return shards::CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return shards::CoreInfo::AnyType; }
  static SHOptionalString help() { return SHCCSTR(""); }

  PARAM_VAR(_space, "Space",
            "This determines in which space the constraint is setup, all other properties should be in the specified space",
            {ConstraintSpaceEnumInfo::Type});
  PARAM_PARAMVAR(_refA, "FirstPoint", "Body 1 constraint reference frame (space determined by Space)",
                 {CoreInfo::Float3Type, CoreInfo::Float3VarType});
  PARAM_PARAMVAR(_refB, "SecondPoint", "Body 2 constraint reference frame (space determined by Space)",
                 {CoreInfo::Float3Type, CoreInfo::Float3VarType});

  PARAM_PARAMVAR(_sliderAxis, "SliderAxis", "Axis along which movement is possible (direction)",
                 {CoreInfo::Float3Type, CoreInfo::Float3VarType});
  PARAM_PARAMVAR(_normalAxis, "NormalAxis", "Perpendicular vector to define the frame",
                 {CoreInfo::Float3Type, CoreInfo::Float3VarType});

  PARAM_PARAMVAR(_limitsMin, "LimitsMin", "Minimum limit of movement", {CoreInfo::FloatType, CoreInfo::FloatVarType});
  PARAM_PARAMVAR(_limitsMax, "LimitsMax", "Maximum limit of movement", {CoreInfo::FloatType, CoreInfo::FloatVarType});

  PARAM_PARAMVAR(_limitSpring, "LimitSpring", "When set, makes the limits soft", SpringSettings::TypesVarOrNone);

  PARAM_PARAMVAR(_maxFrictionForce, "MaxFrictionForce",
                 "Maximum amount of friction force to apply (N) when not driven by a motor",
                 {CoreInfo::FloatType, CoreInfo::FloatVarType});

  PARAM_PARAMVAR(_motorSettings, "MotorSettings", "Motor settings around the sliding axis", {MotorSettings::TypesVarOrNone});
  PARAM_PARAMVAR(_motorSpringSettings, "MotorSpringSettings", "Motor settings around the sliding axis",
                 {SpringSettings::TypesVarOrNone});

  PARAM_IMPL_DERIVED(ConstraintBase, PARAM_IMPL_FOR(_space), PARAM_IMPL_FOR(_refA), PARAM_IMPL_FOR(_refB),
                     PARAM_IMPL_FOR(_sliderAxis), PARAM_IMPL_FOR(_normalAxis), PARAM_IMPL_FOR(_limitsMin),
                     PARAM_IMPL_FOR(_limitsMax), PARAM_IMPL_FOR(_limitSpring), PARAM_IMPL_FOR(_maxFrictionForce),
                     PARAM_IMPL_FOR(_motorSettings), PARAM_IMPL_FOR(_motorSpringSettings));

  SliderConstraint() {
    _space = Var::Enum(JPH::EConstraintSpace::LocalToBodyCOM, ConstraintSpaceEnumInfo::Type);
    _refA = toVar(float3(0.0f));
    _refB = toVar(float3(0.0f));
    _sliderAxis = toVar(float3(1.0f, 0.0f, 0.0f));
    _normalAxis = toVar(float3(0.0f, 1.0f, 0.0f));
    _limitsMin = Var(std::numeric_limits<float>::min());
    _limitsMax = Var(std::numeric_limits<float>::max());
    _limitSpring = SpringSettings::Default;
    _maxFrictionForce = Var(0.0f);
    _motorSettings = SpringSettings::Default;
    _motorSpringSettings = SpringSettings::Default;
  }

  void warmup(SHContext *context) {
    PARAM_WARMUP(context);
    baseWarmup(context);
  }
  void cleanup(SHContext *context) {
    PARAM_CLEANUP(context);
    baseCleanup(context);
  }

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    baseCompose(data, _requiredVariables);
    return data.inputType;
  }

  SHVar activate(SHContext *shContext, const SHVar &input) { return baseActivate(shContext, input, *this); }

  using Params = SetOnCreateConstraintParams<JPH::SliderConstraintSettings>;
  std::shared_ptr<ConstraintNode> createNode(const std::shared_ptr<Core> &core) {
    return core->createNewConstraintNode<Params>();
  }
  void updateNodeParams(const std::shared_ptr<ConstraintNode> &nd) {
    auto &params = nd->getParamsAs<Params>();
    params->settings->mSpace = (JPH::EConstraintSpace)_space.payload.enumValue;
    params->settings->mPoint1 = toJPHVec3(_refA.get().payload.float3Value);
    params->settings->mSliderAxis1 = toJPHVec3(_sliderAxis.get().payload.float3Value);
    params->settings->mNormalAxis1 = toJPHVec3(_normalAxis.get().payload.float3Value);
    params->settings->mPoint2 = toJPHVec3(_refB.get().payload.float3Value);
    params->settings->mSliderAxis2 = toJPHVec3(_sliderAxis.get().payload.float3Value);
    params->settings->mNormalAxis2 = toJPHVec3(_normalAxis.get().payload.float3Value);
    params->settings->mLimitsMin = _limitsMin.get().payload.floatValue;
    params->settings->mLimitsMax = _limitsMax.get().payload.floatValue;
    params->settings->mLimitsSpringSettings = SpringSettings().from(_limitSpring.get());
    params->settings->mMaxFrictionForce = _maxFrictionForce.get().payload.floatValue;
    params->settings->mMotorSettings = MotorSettings().from(_motorSettings.get());
    params->settings->mMotorSettings.mSpringSettings = SpringSettings().from(_motorSpringSettings.get());
    params->updateParamHash0();
  }
};

} // namespace shards::Physics

SHARDS_REGISTER_FN(constraints) {
  using namespace shards::Physics;
  REGISTER_ENUM(ConstraintSpaceEnumInfo);
  REGISTER_SHARD("Physics.FixedConstraint", FixedConstraint);
  REGISTER_SHARD("Physics.DistanceConstraint", DistanceConstraint);
  REGISTER_SHARD("Physics.SliderConstraint", SliderConstraint);
}