#include "anim/path.hpp"
#include "anim/types.hpp"
#include <shards/core/shared.hpp>
#include "linalg.h"
#include <shards/linalg_shim.hpp>
#include <shards/modules/core/math.hpp>
#include <shards/modules/core/time.hpp>
#include <shards/modules/gfx/shards_utils.hpp>
#include <shards/math_ops.hpp>
#include <shards/shardwrapper.hpp>
#include <shards/core/params.hpp>
#include <shards/utility.hpp>
#include <cmath>
#include <chrono>
#include <optional>
#include <stdexcept>

namespace shards::Animations {
using namespace linalg::aliases;
using shards::Time::DeltaTimer;

static auto getKeyframeTime(const SHVar &keyframe) { return (float)((TableVar &)keyframe).get<Var>(Var("Time")); };
static auto getKeyframeValue(const SHVar &keyframe) { return ((TableVar &)keyframe).get<Var>(Var("Value")); };
static auto getKeyframeInterpolation(const SHVar &keyframe) {
  Var &v = ((TableVar &)keyframe).get<Var>(Var("Interpolation"));
  if (v.valueType == SHType::Enum) {
    gfx::checkEnumType(v, ShardsTypes::InterpolationEnumInfo::Type, "Interpolation");
    return (Interpolation)v.payload.enumValue;
  }
  return Interpolation::Linear;
};
static float getAnimationDuration(const SHVar &animation) {
  float duration{};
  for (auto &track : ((SeqVar &)animation)) {
    auto &keyframes = ((TableVar &)track).get<SeqVar>(Var("Frames"));
    if (keyframes.size() > 0) {
      auto &last = keyframes.data()[keyframes.size() - 1];
      duration = std::max(duration, getKeyframeTime(last));
    }
  }
  return duration;
}

struct TimerShard {
  static SHOptionalString help() { return SHCCSTR(R"(Keeps track of an animation timer, based on the given animation)"); }
  static SHTypesInfo inputTypes() { return CoreInfo::NoneType; }
  static SHTypesInfo outputTypes() { return CoreInfo::FloatType; }

  PARAM_PARAMVAR(_animation, "Animation", "The to take the duration from",
                 {ShardsTypes::AnimationOrAnimationVar, {CoreInfo::NoneType}});
  PARAM_PARAMVAR(_duration, "Duration", "The duration of the timer, the timer will loop or stop after reaching this value",
                 {CoreInfo::NoneType, CoreInfo::FloatType, CoreInfo::FloatVarType});
  PARAM_PARAMVAR(_looped, "Looped", "Loop the timer after reaching the target time",
                 {CoreInfo::NoneType, CoreInfo::BoolType, CoreInfo::BoolVarType});
  PARAM_PARAMVAR(_rate, "Rate", "The playback rate", {CoreInfo::NoneType, CoreInfo::FloatType, CoreInfo::FloatVarType});
  PARAM_PARAMVAR(_offset, "Offset", "Timer offset", {CoreInfo::NoneType, CoreInfo::FloatType, CoreInfo::FloatVarType});
  PARAM_PARAMVAR(_variable, "Variable", "The variable to store the result in", {CoreInfo::NoneType, CoreInfo::FloatVarType});
  PARAM(ShardsVar, _action, "Action", "The action to evaluate after the given Duration",
        {CoreInfo::Shards, {CoreInfo::NoneType}});
  PARAM_IMPL(PARAM_IMPL_FOR(_animation), PARAM_IMPL_FOR(_duration), PARAM_IMPL_FOR(_looped), PARAM_IMPL_FOR(_rate),
             PARAM_IMPL_FOR(_offset), PARAM_IMPL_FOR(_action), PARAM_IMPL_FOR(_variable));

  double _internalTime{};
  bool _hasCallback{};
  bool _stopped{};
  DeltaTimer _deltaTimer;

  enum class DurationSource { Animation, Infinite, Var } _durationSource = DurationSource::Infinite;

  TimerShard() {
    _looped = Var{true};
    _rate = Var{1.0f};
  }

  void warmup(SHContext *context) {
    PARAM_WARMUP(context);
    _internalTime = 0.0f;
    _deltaTimer.reset();
  }

  double &getTime() { return _variable.isVariable() ? _variable.get().payload.floatValue : _internalTime; }

  void cleanup(SHContext *context) { PARAM_CLEANUP(context); }

  PARAM_REQUIRED_VARIABLES()
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);

    _durationSource = DurationSource::Infinite;
    const SHExposedTypeInfo *animationType = findParamVarExposedType(data, _animation);
    if (animationType) {
      if (_duration->valueType != SHType::None) {
        throw std::runtime_error(fmt::format("Can not specify both Duration an Animation as a source for duration"));
      }
      _durationSource = DurationSource::Animation;
    }

    if (_duration->valueType != SHType::None)
      _durationSource = DurationSource::Var;

    _hasCallback = _action.shards().len > 0;
    if (_hasCallback) {
      _action.compose(data);
    }

    return outputTypes().elements[0];
  }

  std::optional<float> getDuration() {
    switch (_durationSource) {
    case DurationSource::Animation:
      return getAnimationDuration(_animation.get());
    case DurationSource::Var:
      return (float)Var{_duration.get()};
    default:
    case DurationSource::Infinite:
      return std::nullopt;
    }
  }

  void runAction(SHContext *shContext, const SHVar &input) {
    if (_hasCallback) {
      SHVar _sink{};
      _action.activate(shContext, input, _sink);
    }
  }

  SHVar activate(SHContext *shContext, const SHVar &input) {
    Var offsetVar{_offset.get()};
    float offset = offsetVar.isNone() ? 0.0f : float(offsetVar);

    Var rateVar{_rate.get()};
    float rate = rateVar.isNone() ? 1.0f : float(rateVar);

    Var durationVar{_duration.get()};
    std::optional<float> duration = getDuration();

    Var loopedVar{_looped.get()};
    bool looped = loopedVar.isNone() ? true : bool(loopedVar);
    if (!duration)
      looped = false;

    double &time = getTime();

    // Auto restart with variable
    if (_variable.isVariable()) {
      if (!duration || time < *duration) {
        _stopped = false;
      }
    }

    float deltaTime = _deltaTimer.update();
    if (!_stopped) {
      time += deltaTime * rate;
    }

    float evalTime = time + offset;
    if (duration && !_stopped) {
      if (evalTime > duration.value()) {
        runAction(shContext, input);
        if (looped) {
          evalTime = std::fmod(evalTime, duration.value());
          time = std::fmod(time + offset, duration.value()) - offset;
        } else {
          _stopped = true;
        }
        evalTime = std::min(duration.value(), evalTime);
      }
    }

    return Var{evalTime};
  }
};

struct PlayShard {
  static SHOptionalString help() { return SHCCSTR(R"(Outputs the duration of an animation, in seconds)"); }

  static inline shards::Types OutputTypes{{Type::SeqOf(ShardsTypes::ValueTable)}};

  static SHTypesInfo inputTypes() { return CoreInfo::FloatType; }
  static SHTypesInfo outputTypes() { return SHTypesInfo(OutputTypes); }

  static SHOptionalString inputHelp() { return SHCCSTR(R"(The position in the animation to play)"); }
  static SHOptionalString outputHelp() { return SHCCSTR(R"(The interpolated frame data)"); }

  PARAM_PARAMVAR(_animation, "Animation", "The animation to play", ShardsTypes::AnimationOrAnimationVar);
  PARAM_IMPL(PARAM_IMPL_FOR(_animation));

  SeqVar _output;

  void warmup(SHContext *context) { PARAM_WARMUP(context); }
  void cleanup(SHContext *context) { PARAM_CLEANUP(context); }

  PARAM_REQUIRED_VARIABLES()
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    return outputTypes().elements[0];
  }

  void evaluateTrack(SHVar &outputValue, const SeqVar &keyframesVar, float time) const {
    SeqVar &keyframes = (SeqVar &)keyframesVar;
    if (keyframes.empty())
      return;

    auto it = std::lower_bound(keyframes.begin(), keyframes.end(), time,
                               [](const SHVar &a, const auto &b) { return getKeyframeTime(a) < b; });

    if (it == keyframes.end()) {
      outputValue = getKeyframeValue(keyframes.back());
      return;
    }
    size_t indexB = std::distance(keyframes.begin(), it);
    size_t indexA = indexB - 1;
    if (indexA == size_t(~0)) {
      outputValue = getKeyframeValue(keyframes[indexB]);
      return;
    }

    auto va = getKeyframeValue(keyframes[indexA]);
    auto interpolation = getKeyframeInterpolation(keyframes[indexB]);

    if (interpolation == Interpolation::Linear) {
      float timeA = getKeyframeTime(keyframes[indexA]);
      float timeB = getKeyframeTime(*it);
      float phase = (time - timeA) / (timeB - timeA);

      auto vb = getKeyframeValue(keyframes[indexB]);

      if (va.valueType != vb.valueType)
        throw std::runtime_error("Can not interpolate between two different values");

      // TODO: better determination
      bool isQuaternion = va.valueType == SHType::Float4;
      if (isQuaternion) {
        // Quaternion slerp
        float4 a = toVec<float4>(va);
        float4 b = toVec<float4>(vb);
        outputValue = toVar(linalg::qslerp(a, b, phase));
      } else {
        // Generic lerp
        outputValue.valueType = va.valueType;
        Math::dispatchType<Math::DispatchType::NumberTypes>(va.valueType, Math::ApplyLerp{}, outputValue.payload, va.payload,
                                                            vb.payload, double(phase));
      }
    } else {
      outputValue = va;
    }
  }

  SHVar activate(SHContext *shContext, const SHVar &input) {
    _output.clear();

    float time{(Var &)input};
    for (auto &trackVar : _animation.get()) {
      TableVar &trackTable = (TableVar &)trackVar;
      SeqVar &pathVar = trackTable.get<SeqVar>(Var("Path"));

      Path path(pathVar.data(), pathVar.size());

      SHVar value;
      evaluateTrack(value, trackTable.get<SeqVar>(Var("Frames")), time);

      auto &outFrame = _output.emplace_back_table();
      outFrame["Path"] = pathVar;
      outFrame["Value"] = value;
    }

    return _output;
  }
};

struct DurationShard {
  static SHOptionalString help() { return SHCCSTR(R"(Outputs the duration of an animation, in seconds)"); }
  static SHTypesInfo inputTypes() { return ShardsTypes::Animation; }
  static SHTypesInfo outputTypes() { return CoreInfo::FloatType; }

  PARAM_IMPL();
  void warmup(SHContext *context) { PARAM_WARMUP(context); }
  void cleanup(SHContext *context) { PARAM_CLEANUP(context); }

  PARAM_REQUIRED_VARIABLES()
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    return outputTypes().elements[0];
  }

  SHVar activate(SHContext *shContext, const SHVar &input) { return Var{getAnimationDuration(input)}; }
};

struct InterpolateShard {
  static inline shards::Types FloatTypes{CoreInfo::FloatType, CoreInfo::Float2Type, CoreInfo::Float3Type, CoreInfo::Float4Type};

  static SHOptionalString help() { return SHCCSTR(R"(Outputs the duration of an animation, in seconds)"); }
  static SHTypesInfo inputTypes() { return FloatTypes; }
  static SHTypesInfo outputTypes() { return FloatTypes; }

  DeltaTimer _deltaTimer;
  std::optional<Var> _lastValue;
  Var _a;
  Var _b;
  Var _lastOutput;
  double t{};

  PARAM_PARAMVAR(_duration, "Duration", "Duration of interpolation",
                 {CoreInfo::NoneType, CoreInfo::FloatType, CoreInfo::FloatVarType});
  PARAM_IMPL(PARAM_IMPL_FOR(_duration));

  double getDuration() const { return _duration.isNone() ? 1.0f : _duration.get().payload.floatValue; }

  void warmup(SHContext *context) {
    PARAM_WARMUP(context);
    _deltaTimer.reset();
    _lastValue.reset();
    _lastOutput.valueType = SHType::None;
  }
  void cleanup(SHContext *context) { PARAM_CLEANUP(context); }

  PARAM_REQUIRED_VARIABLES()
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    return data.inputType;
  }

  void updateOutput() {
    double phase = t / getDuration();

    // TODO: better determination
    bool isQuaternion = _a.valueType == SHType::Float4;
    if (isQuaternion) {
      // Quaternion slerp
      float4 a = toVec<float4>(_a);
      float4 b = toVec<float4>(_b);
      _lastOutput = toVar(linalg::qslerp(a, b, float(phase)));
    } else {
      // Generic lerp
      _lastOutput.valueType = _a.valueType;
      Math::dispatchType<Math::DispatchType::FloatTypes>(_a.valueType, Math::ApplyLerp{}, _lastOutput.payload, _a.payload,
                                                         _b.payload, double(phase));
    }
  }

  SHVar activate(SHContext *shContext, const SHVar &input) {
    float dt = _deltaTimer.update();
    if (!_lastValue) {
      _lastValue = input;
      _lastOutput = input;
      _a = input;
      _b = input;
      t = getDuration();
    }

    if (*_lastValue != input) {
      _lastValue = input;
      _a = _lastOutput;
      _b = input;
      t = 0.0f;
    }

    if (t < getDuration()) {
      t = std::min((t + dt), getDuration());
      updateOutput();
    }

    return _lastOutput;
  }
};

extern void registerTypes();
} // namespace shards::Animations

SHARDS_REGISTER_FN(anim) {
  using namespace shards::Animations;
  REGISTER_ENUM(ShardsTypes::InterpolationEnumInfo);
  REGISTER_SHARD("Animation.Timer", TimerShard);
  REGISTER_SHARD("Animation.Play", PlayShard);
  REGISTER_SHARD("Animation.Duration", DurationShard);
  REGISTER_SHARD("Animation.Interpolated", InterpolateShard);
}
