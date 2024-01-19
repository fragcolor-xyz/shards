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
  PARAM(ShardsVar, _action, "Action", "The action to evaluate after the given Duration",
        {CoreInfo::Shards, {CoreInfo::NoneType}});
  PARAM_IMPL(PARAM_IMPL_FOR(_animation), PARAM_IMPL_FOR(_duration), PARAM_IMPL_FOR(_looped), PARAM_IMPL_FOR(_rate),
             PARAM_IMPL_FOR(_offset), PARAM_IMPL_FOR(_action));

  float _time{};
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
    _time = 0.0f;
    _deltaTimer.reset();
  }

  void cleanup(SHContext* context) { PARAM_CLEANUP(context); }

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

    float deltaTime = _deltaTimer.update();
    if (!_stopped) {
      _time += deltaTime * rate;
    }

    float evalTime = _time + offset;
    if (duration && !_stopped) {
      if (evalTime > duration.value()) {
        runAction(shContext, input);
        if (looped) {
          evalTime = std::fmod(evalTime, duration.value());
          _time = std::fmod(_time + offset, duration.value()) - offset;
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
  void cleanup(SHContext* context) { PARAM_CLEANUP(context); }

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

        // Fix for long path rotations (>180 degrees)
        float dot = linalg::dot(a, b);
        if (dot < 0.0)
          a = -a;
        outputValue = toVar(linalg::slerp(a, b, phase));
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

      TableVar outFrame;
      outFrame.get<OwnedVar>(Var("Path")) = pathVar;
      outFrame.get<OwnedVar>(Var("Value")) = value;

      (TableVar &)_output.emplace_back_fast() = std::move(outFrame);
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
  void cleanup(SHContext* context) { PARAM_CLEANUP(context); }

  PARAM_REQUIRED_VARIABLES()
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    return outputTypes().elements[0];
  }

  SHVar activate(SHContext *shContext, const SHVar &input) { return Var{getAnimationDuration(input)}; }
};

extern void registerTypes();
} // namespace shards::Animations

SHARDS_REGISTER_FN(anim) {
  using namespace shards::Animations;
  REGISTER_ENUM(ShardsTypes::InterpolationEnumInfo);
  REGISTER_SHARD("Animation.Timer", TimerShard);
  REGISTER_SHARD("Animation.Play", PlayShard);
  REGISTER_SHARD("Animation.Duration", DurationShard);
}
