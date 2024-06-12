/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2019 Fragcolor Pte. Ltd. */

#include <shards/core/shared.hpp>
#include <shards/core/params.hpp>
#include <shards/gfx/moving_average.hpp>
#include "time.hpp"

namespace shards {
namespace Time {
struct Now {
  static inline ProcessClock _clock{};

  static SHTypesInfo inputTypes() { return CoreInfo::NoneType; }
  static SHTypesInfo outputTypes() { return CoreInfo::FloatType; }

  SHVar activate(SHContext *context, const SHVar &input) {
    auto tnow = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> dt = tnow - _clock.Start;
    return Var(dt.count());
  }
};

struct NowMs : public Now {
  SHVar activate(SHContext *context, const SHVar &input) {
    auto tnow = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> dt = tnow - _clock.Start;
    return Var(dt.count());
  }
};

struct Delta {
  DeltaTimer _deltaTimer;

  static SHOptionalString help() {
    return SHCCSTR(R"(Gives the time between the last activation of this shard and the current, capped to a limit)");
  }

  static SHTypesInfo inputTypes() { return CoreInfo::NoneType; }
  static SHTypesInfo outputTypes() { return CoreInfo::FloatType; }

  void warmup(SHContext *context) { _deltaTimer.reset(); }

  SHVar activate(SHContext *shContext, const SHVar &input) {
    float dt = _deltaTimer.update();
    return Var{dt};
  }
};

struct DeltaMs : public Delta {
  SHVar activate(SHContext *context, const SHVar &input) {
    using Type = std::chrono::duration<double, std::milli>;
    return Var{_deltaTimer.update<Type>()};
  }
};

struct EpochMs {
  static SHTypesInfo inputTypes() { return CoreInfo::NoneType; }
  static SHTypesInfo outputTypes() { return CoreInfo::IntType; }

  SHVar activate(SHContext *context, const SHVar &input) {
    using namespace std::chrono;
    milliseconds ms = duration_cast<milliseconds>(system_clock::now().time_since_epoch());
    return Var(int64_t(ms.count()));
  }
};

struct Epoch {
  static SHTypesInfo inputTypes() { return CoreInfo::NoneType; }
  static SHTypesInfo outputTypes() { return CoreInfo::IntType; }

  SHVar activate(SHContext *context, const SHVar &input) {
    using namespace std::chrono;
    seconds ms = duration_cast<seconds>(system_clock::now().time_since_epoch());
    return Var(int64_t(ms.count()));
  }
};

struct Pop {
  static inline Types PSeqTypes{{CoreInfo::AnyType, CoreInfo::IntType}};
  static inline Type PSeqType = Type::SeqOf(PSeqTypes);
  static inline Type VPSeqType = Type::VariableOf(PSeqTypes);
  static SHTypesInfo inputTypes() { return CoreInfo::NoneType; }
  static SHTypesInfo outputTypes() { return CoreInfo::AnyType; }
  static inline Parameters Params{{"Sequence",
                                   SHCCSTR("A variables sequence of pairs [value "
                                           "pop-epoch-time-ms] with types [Any SHType::Int]"),
                                   {VPSeqType, CoreInfo::NoneType}}};

  SHOptionalString help() {
    return SHCCSTR("This shards delays its output until one of the values of "
                   "the sequence parameter expires.");
  }

  ParamVar _pseq{};
  OwnedVar _last{};

  SHParametersInfo parameters() { return Params; }

  void setParam(int index, const SHVar &value) { _pseq = value; }

  SHVar getParam(int index) { return _pseq; }

  SHTypeInfo compose(const SHInstanceData &data) {
    if (!_pseq.isVariable())
      throw ComposeError("Time.Pop expects a variable");

    for (auto info : data.shared) {
      if (strcmp(info.name, _pseq.variableName()) == 0) {
        if (info.exposedType.basicType != SHType::Seq || info.exposedType.seqTypes.len != 2)
          throw ComposeError("Time.Pop expected a sequence of pairs");

        if (!info.isMutable)
          throw ComposeError("Time.Pop expects a mutable sequence variable");

        if (info.exposedType.seqTypes.elements[1] != CoreInfo::IntType)
          throw ComposeError("Time.Pop expects a pair of [Any SHType::Int] values");

        return info.exposedType.seqTypes.elements[0];
      }
    }

    throw ComposeError("Time.Pop variable not found");
  }

  void warmup(SHContext *context) { _pseq.warmup(context); }

  void cleanup(SHContext* context) { _pseq.cleanup(); }

  SHVar activate(SHContext *context, const SHVar &input) {
    using namespace std::chrono;
    while (true) {
      auto &seq = _pseq.get();
      milliseconds ms = duration_cast<milliseconds>(system_clock::now().time_since_epoch());
      auto now = ms.count();
      for (uint32_t idx = 0; idx < seq.payload.seqValue.len; idx++) {
        auto &v = seq.payload.seqValue.elements[idx];
        auto &time = v.payload.seqValue.elements[1]; // time ms in epoch here
        if (now >= time.payload.intValue) {
          _last = v.payload.seqValue.elements[0];
          // remove the item fast as order does not matter, time deadline is the
          // only thing that matters)
          arrayDelFast(seq.payload.seqValue, idx);
          return _last;
        }
      }
      // if we are here means we need to wait
      SH_SUSPEND(context, 0);
    }
  }
};

struct ToString {
  static SHOptionalString help() { return SHCCSTR("Converts time into a human readable string."); }
  static SHTypesInfo inputTypes() { return _inputTypes; }
  static SHOptionalString inputHelp() { return SHCCSTR("The time to convert."); }
  static SHTypesInfo outputTypes() { return CoreInfo::StringType; }
  static SHOptionalString outputHelp() { return SHCCSTR("A string representation of the time."); }

  SHParametersInfo parameters() { return _params; }

  void setParam(int index, const SHVar &value) { _isMillis = value.payload.boolValue; }

  SHVar getParam(int index) { return Var(_isMillis); }

  SHVar activate(SHContext *context, const SHVar &input) {

    switch (input.valueType) {
    case SHType::Int: {
      _output = _isMillis ? format(std::chrono::duration<int64_t, std::milli>(input.payload.intValue))
                          : format(std::chrono::duration<int64_t>(input.payload.intValue));
    } break;
    case SHType::Float: {
      _output = _isMillis ? format(std::chrono::duration<double, std::milli>(input.payload.floatValue))
                          : format(std::chrono::duration<double>(input.payload.floatValue));
    } break;
    default: {
      SHLOG_ERROR("Unexpected type for pure Time conversion: {}", type2Name(input.valueType));
      throw ActivationError("Type not supported for timeN conversion");
    }
    }

    return Var(_output);
  }

private:
  static inline Types _inputTypes = {{CoreInfo::IntType, CoreInfo::FloatType}};
  static inline Parameters _params{
      {"Millis", SHCCSTR("True if the input is given in milliseconds, False if given in seconds."), {CoreInfo::BoolType}}};

  bool _isMillis{false};
  std::string _output;

  template <typename T> inline std::string format(T timeunit) {
    using namespace std::chrono;

    milliseconds ms = duration_cast<milliseconds>(timeunit);
    std::ostringstream os;
    bool foundNonZero = false;
    os.fill('0');
    typedef duration<int, std::ratio<86400 * 365>> years;
    const auto y = duration_cast<years>(ms);
    if (y.count()) {
      foundNonZero = true;
      os << y.count() << "y:";
      ms -= y;
    }
    typedef duration<int, std::ratio<86400>> days;
    const auto d = duration_cast<days>(ms);
    if (d.count()) {
      foundNonZero = true;
      os << d.count() << "d:";
      ms -= d;
    }
    const auto h = duration_cast<hours>(ms);
    if (h.count() || foundNonZero) {
      foundNonZero = true;
      os << h.count() << "h:";
      ms -= h;
    }
    const auto m = duration_cast<minutes>(ms);
    if (m.count() || foundNonZero) {
      foundNonZero = true;
      os << m.count() << "m:";
      ms -= m;
    }
    const auto s = duration_cast<seconds>(ms);
#if !TIME_TOSTRING_PRINT_MILLIS
    os << s.count() << "s";
#else
    if (s.count() || foundNonZero) {
      foundNonZero = true;
      os << s.count() << "s:";
      ms -= s;
    }
    os << std::setw(3) << ms.count() << "ms";
#endif
    return os.str();
  }
};

struct MovingAverage {
  std::optional<gfx::MovingAverage<double>> _ma{};

  static SHOptionalString help() { return SHCCSTR("Computes a moving average of a single floating point number."); }
  static SHTypesInfo inputTypes() { return CoreInfo::FloatType; }
  static SHTypesInfo outputTypes() { return CoreInfo::FloatType; }

  PARAM_VAR(_windowSize, "Window", "The moving average window length (in frames)", {CoreInfo::IntType});
  PARAM_IMPL(PARAM_IMPL_FOR(_windowSize))

  MovingAverage() { _windowSize = Var(16); }

  void warmup(SHContext *context) { _ma.emplace(_windowSize.payload.intValue); }

  SHVar activate(SHContext *context, const SHVar &input) {
    _ma->add(input.payload.floatValue);
    return Var(_ma->getAverage());
  }
};

} // namespace Time
SHARDS_REGISTER_FN(time) {
  REGISTER_SHARD("Time.Now", Time::Now);
  REGISTER_SHARD("Time.NowMs", Time::NowMs);
  REGISTER_SHARD("Time.Delta", Time::Delta);
  REGISTER_SHARD("Time.DeltaMs", Time::DeltaMs);
  REGISTER_SHARD("Time.EpochMs", Time::EpochMs);
  REGISTER_SHARD("Time.Epoch", Time::Epoch);
  REGISTER_SHARD("Time.Pop", Time::Pop);
  REGISTER_SHARD("Time.ToString", Time::ToString);
  REGISTER_SHARD("Time.MovingAverage", Time::MovingAverage);
}
} // namespace shards
