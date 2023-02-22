#include "anim/path.hpp"
#include "anim/types.hpp"
#include "common_types.hpp"
#include "foundation.hpp"
#include "linalg.h"
#include "linalg_shim.hpp"
#include "shards.h"
#include "shards.hpp"
#include "shards/math.hpp"
#include "shardwrapper.hpp"
#include "types.hpp"
#include "params.hpp"
#include "utility.hpp"
#include "shards/math_ops.hpp"
#include <cmath>
#include <chrono>
#include <optional>
#include <stdexcept>

namespace shards::Animations {
using namespace linalg::aliases;

static auto getKeyframeTime(const SHVar &keyframe) { return (float)((TableVar &)keyframe).get<Var>("Time"); };
static auto getKeyframeValue(const SHVar &keyframe) { return ((TableVar &)keyframe).get<Var>("Value"); };
static auto getKeyframeInterpolation(const SHVar &keyframe) {
  Var &v = ((TableVar &)keyframe).get<Var>("Interpolation");
  if (v.valueType == SHType::Enum) {
    return (Interpolation)v.payload.enumValue;
  }
  return Interpolation::Linear;
};
static float getAnimationDuration(const SHVar &animation) {
  float duration{};
  for (auto &track : ((SeqVar &)animation)) {
    auto &keyframes = ((TableVar &)track).get<SeqVar>("Frames");
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
                 {Types::AnimationOrAnimationVar, {CoreInfo::NoneType}});
  PARAM_PARAMVAR(_duration, "Duration", "The duration of the timer, the timer will loop or stop after reaching this value",
                 {CoreInfo::NoneType, CoreInfo::FloatType, CoreInfo::FloatVarType});
  PARAM_PARAMVAR(_looped, "Looped", "Loop the timer after reaching the target time",
                 {CoreInfo::NoneType, CoreInfo::BoolType, CoreInfo::BoolVarType});
  PARAM_PARAMVAR(_rate, "Rate", "The playback rate", {CoreInfo::NoneType, CoreInfo::FloatType, CoreInfo::FloatVarType});
  PARAM_PARAMVAR(_offset, "Offset", "Timer offset", {CoreInfo::NoneType, CoreInfo::FloatType, CoreInfo::FloatVarType});
  PARAM(ShardsVar, _action, "Action", "The action to evaluate after the given Duration",
        {CoreInfo::Shards, {CoreInfo::NoneType}});
  PARAM_IMPL(TimerShard, PARAM_IMPL_FOR(_animation), PARAM_IMPL_FOR(_duration), PARAM_IMPL_FOR(_looped), PARAM_IMPL_FOR(_rate),
             PARAM_IMPL_FOR(_offset), PARAM_IMPL_FOR(_action));

  using Clock = std::chrono::high_resolution_clock;
  using TimePoint = std::chrono::time_point<Clock>;
  using FloatSecDuration = std::chrono::duration<float>;

  TimePoint _lastActivation;
  float _time{};
  bool _hasCallback{};
  bool _stopped{};

  enum class DurationSource { Animation, Infinite, Var } _durationSource = DurationSource::Infinite;

  // Limit delta time to avoid jumps after unpausing wires
  const float MaxDeltaTime = 1.0f / 15.0f;

  TimerShard() {
    _looped = Var{true};
    _rate = Var{1.0f};
  }

  void warmup(SHContext *context) {
    PARAM_WARMUP(context);
    _time = 0.0f;
    _lastActivation = Clock::now();
  }

  void cleanup() { PARAM_CLEANUP(); }

  SHTypeInfo compose(SHInstanceData &data) {
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

    auto now = Clock::now();
    if (!_stopped) {
      FloatSecDuration delta = (now - _lastActivation);
      _time += std::min(MaxDeltaTime, delta.count()) * rate;
    }
    _lastActivation = now;

    float evalTime = _time + offset;
    if (duration && !_stopped) {
      if (evalTime > duration.value()) {
        runAction(shContext, input);
        if (looped) {
          evalTime = std::fmodf(evalTime, duration.value());
          _time = std::fmodf(_time + offset, duration.value()) - offset;
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
  static SHOptionalString help() { return SHCCSTR(R"(Returns the duration of an animation, in seconds)"); }

  static inline shards::Types OutputTypes{{Type::SeqOf(Types::ValueTable)}};

  static SHTypesInfo inputTypes() { return CoreInfo::FloatType; }
  static SHTypesInfo outputTypes() { return SHTypesInfo(OutputTypes); }

  static SHOptionalString inputHelp() { return SHCCSTR(R"(The position in the animation to play)"); }
  static SHOptionalString outputHelp() { return SHCCSTR(R"(The interpolated frame data)"); }

  PARAM_PARAMVAR(_animation, "Animation", "The animation to play", Types::AnimationOrAnimationVar);
  PARAM_IMPL(PlayShard, PARAM_IMPL_FOR(_animation));

  SeqVar _output;

  void warmup(SHContext *context) { PARAM_WARMUP(context); }
  void cleanup() { PARAM_CLEANUP(); }

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
    if (indexA == ~0) {
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
      SeqVar &pathVar = trackTable.get<SeqVar>("Path");

      Path path(pathVar.data(), pathVar.size());

      SHVar value;
      evaluateTrack(value, trackTable.get<SeqVar>("Frames"), time);

      TableVar outFrame;
      outFrame.get<OwnedVar>("Path") = pathVar;
      outFrame.get<OwnedVar>("Value") = value;

      (TableVar &)_output.emplace_back() = std::move(outFrame);
    }

    return _output;
  }
};

struct DurationShard {
  static SHOptionalString help() { return SHCCSTR(R"(Returns the duration of an animation, in seconds)"); }
  static SHTypesInfo inputTypes() { return Types::Animation; }
  static SHTypesInfo outputTypes() { return CoreInfo::FloatType; }

  PARAM_IMPL(DurationShard);
  void warmup(SHContext *context) { PARAM_WARMUP(context); }
  void cleanup() { PARAM_CLEANUP(); }
  SHTypeInfo compose(SHInstanceData &data) { return outputTypes().elements[0]; }
  SHVar activate(SHContext *shContext, const SHVar &input) { return Var{getAnimationDuration(input)}; }
};

extern void registerTypes();
void registerShards() {
  registerTypes();
  REGISTER_SHARD("Animation.Timer", TimerShard);
  REGISTER_SHARD("Animation.Play", PlayShard);
  REGISTER_SHARD("Animation.Duration", DurationShard);
}
} // namespace shards::Animations