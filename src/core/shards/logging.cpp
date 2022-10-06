/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2019 Fragcolor Pte. Ltd. */

#include "shared.hpp"
#include <numeric>
#include <string>
#include <log/log.hpp>
#include <spdlog/sinks/dist_sink.h>

namespace shards {
struct LoggingBase {
  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }

  static SHTypesInfo outputTypes() { return CoreInfo::AnyType; }
};

struct Log : public LoggingBase {
  static inline ParamsInfo msgParamsInfo =
      ParamsInfo(ParamsInfo::Param("Prefix", SHCCSTR("The message to prefix to the logged output."), CoreInfo::StringType));

  std::string msg;

  static SHParametersInfo parameters() { return SHParametersInfo(msgParamsInfo); }

  static SHOptionalString help() {
    return SHCCSTR(
        "Logs the output of a shard or the value of a variable to the console (along with an optional prefix string).");
  }

  void setParam(int index, const SHVar &inValue) {
    switch (index) {
    case 0:
      msg = inValue.payload.stringValue;
      break;
    default:
      break;
    }
  }

  SHVar getParam(int index) {
    auto res = SHVar();
    switch (index) {
    case 0:
      res.valueType = String;
      res.payload.stringValue = msg.c_str();
      break;
    default:
      break;
    }
    return res;
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    auto current = context->wireStack.back();
    if (msg.size() > 0) {
      SHLOG_INFO("[{}] {}: {}", current->name, msg, input);
    } else {
      SHLOG_INFO("[{}] {}", current->name, input);
    }
    return input;
  }
};

struct Msg : public LoggingBase {
  static inline ParamsInfo msgParamsInfo = ParamsInfo(
      ParamsInfo::Param("Message", SHCCSTR("The message to display on the user's screen or console."), CoreInfo::StringType));

  std::string msg;

  static SHParametersInfo parameters() { return SHParametersInfo(msgParamsInfo); }

  static SHOptionalString help() {
    return SHCCSTR("Displays the passed message string or the passed variable's value to the user via standard output.");
  }

  void setParam(int index, const SHVar &inValue) {
    switch (index) {
    case 0:
      msg = inValue.payload.stringValue;
      break;
    default:
      break;
    }
  }

  SHVar getParam(int index) {
    auto res = SHVar();
    switch (index) {
    case 0:
      res.valueType = String;
      res.payload.stringValue = msg.c_str();
      break;
    default:
      break;
    }
    return res;
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    auto current = context->wireStack.back();
    SHLOG_INFO("[{}] {}", current->name, msg);
    return input;
  }
};

/*
 * Custom ring buffer sink
 */
template <typename Mutex> class custom_ringbuffer_sink final : public spdlog::sinks::base_sink<Mutex> {
public:
  explicit custom_ringbuffer_sink(size_t n_items, spdlog::level::level_enum min_level = spdlog::level::debug)
      : q_{n_items}, min_level_(min_level) {}

  inline bool is_dirty() { return _dirty; }

  std::vector<std::string> get_last_formatted() {
    if (_dirty) {
      std::lock_guard<Mutex> lock(spdlog::sinks::base_sink<Mutex>::mutex_);
      _dirty = false;

      auto n_items = q_.size();
      _formatted_cache.clear();
      _formatted_cache.reserve(n_items);
      for (size_t i = 0; i < n_items; i++) {
        spdlog::memory_buf_t formatted;
        spdlog::sinks::base_sink<Mutex>::formatter_->format(q_.at(i), formatted);
        _formatted_cache.push_back(fmt::to_string(formatted));
      }
    }

    return _formatted_cache;
  }

protected:
  void sink_it_(const spdlog::details::log_msg &msg) override {
    if (msg.level >= min_level_) {
      _dirty = true;
      q_.push_back(spdlog::details::log_msg_buffer{msg});
    }
  }
  void flush_() override {}

private:
  spdlog::details::circular_q<spdlog::details::log_msg_buffer> q_;
  spdlog::level::level_enum min_level_;

  bool _dirty;
  std::vector<std::string> _formatted_cache;
};

using custom_ringbuffer_sink_mt = custom_ringbuffer_sink<std::mutex>;
using custom_ringbuffer_sink_st = custom_ringbuffer_sink<spdlog::details::null_mutex>;

struct CaptureLog {
  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }

  static SHTypesInfo outputTypes() { return CoreInfo::AnyType; }

  static SHParametersInfo parameters() { return _params; }

  void setParam(int index, const SHVar &value) {
    switch (index) {
    case 0:
      _variable = value;
      break;
    case 1:
      _n_items = size_t(value.payload.intValue);
      break;
    case 2:
      _min_level = value.payload.stringValue;
      break;
    case 3:
      _pattern = value.payload.stringValue;
      break;
    default:
      break;
    }
  }

  SHVar getParam(int index) {
    switch (index) {
    case 0:
      return _variable;
    case 1: {
      auto n_items = SHVar();
      n_items.valueType = Int;
      n_items.payload.intValue = _n_items;
      return n_items;
    }
    case 2: {
      auto min_level = SHVar();
      min_level.valueType = String;
      min_level.payload.stringValue = _min_level.c_str();
      return min_level;
    }
    case 3: {
      auto pattern = SHVar();
      pattern.valueType = String;
      pattern.payload.stringValue = _pattern.c_str();
      return pattern;
    }
    default:
      return Var::Empty;
    }
  }

  SHTypeInfo compose(const SHInstanceData &data) {
    if (_variable.isVariable()) {
      _exposing = true; // assume we expose a new variable
      // search for a possible existing variable and ensure it's the right type
      for (auto &var : data.shared) {
        if (strcmp(var.name, _variable.variableName()) == 0) {
          // we found a variable, make sure it's the right type and mark
          // exposing off
          _exposing = false;
          if (var.exposedType.basicType != SHType::String) {
            throw SHException("CaptureLog - Variable: Existing variable type not matching the input.");
          }
          // also make sure it's mutable!
          if (!var.isMutable) {
            throw SHException("CaptureLog - Variable: Existing variable is not mutable.");
          }
          break;
        }
      }
    }
    return data.inputType;
  }

  SHExposedTypesInfo requiredVariables() {
    if (_variable.isVariable() && !_exposing) {
      _expInfo = ExposedInfo(
          ExposedInfo::Variable(_variable.variableName(), SHCCSTR("The required variable."), SHTypeInfo(CoreInfo::StringType)));
      return SHExposedTypesInfo(_expInfo);
    } else {
      return {};
    }
  }

  SHExposedTypesInfo exposedVariables() {
    if (_variable.isVariable() > 0 && _exposing) {
      _expInfo = ExposedInfo(ExposedInfo::Variable(_variable.variableName(), SHCCSTR("The exposed variable."),
                                                   SHTypeInfo(CoreInfo::StringType), true));
      return SHExposedTypesInfo(_expInfo);
    } else {
      return {};
    }
  }

  void warmup(SHContext *context) {
    auto logger = spdlog::default_logger();
    assert(logger);
    auto sink = logger->sinks().at(0);
    assert(sink);
    auto dist_sink = std::dynamic_pointer_cast<spdlog::sinks::dist_sink_mt>(sink);
    if (dist_sink) {
      _ring = std::make_shared<custom_ringbuffer_sink_mt>(_n_items, spdlog::level::from_str(_min_level));
      _ring->set_formatter(
          std::unique_ptr<spdlog::formatter>(new spdlog::pattern_formatter(_pattern, spdlog::pattern_time_type::local)));
      dist_sink->add_sink(_ring);
    }

    _variable.warmup(context);

    if (_exposing) {
      auto &var = _variable.get();
      var.valueType = SHType::String;
    }
  }

  void cleanup() {
    _variable.cleanup();

    auto logger = spdlog::default_logger();
    assert(logger);
    auto sink = logger->sinks().at(0);
    assert(sink);
    auto dist_sink = std::dynamic_pointer_cast<spdlog::sinks::dist_sink_mt>(sink);
    if (dist_sink) {
      dist_sink->remove_sink(_ring);
      _ring.reset();
    }
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    if (likely((bool)_ring)) {
      if (_ring->is_dirty()) {
        auto msgs = _ring->get_last_formatted();

        _s.clear();
        for (auto i = msgs.begin(); i != msgs.end(); ++i) {
          _s += *i;
        }

        auto &var = _variable.get();
        var.payload.stringValue = _s.data();
        var.payload.stringLen = uint32_t(_s.length());
      }
    }
    return input;
  }

private:
  static inline Parameters _params{
      {"Variable", SHCCSTR("The variable to hold the captured logs."), {CoreInfo::StringVarType}},
      {"Size", SHCCSTR("The maximum number of logs to retain."), {CoreInfo::IntType}},
      {"MinLevel", SHCCSTR("The minimum level of logs to capture."), {CoreInfo::StringType}},
      {"Pattern", SHCCSTR("The pattern used to format the logs."), {CoreInfo::StringType}},
  };

  std::string _s{};
  ParamVar _variable{};
  ExposedInfo _expInfo{};
  bool _exposing = false;

  size_t _n_items{8};
  std::string _min_level{"debug"};
  std::string _pattern{"%^[%l]%$ [%Y-%m-%d %T.%e] [T-%t] [%s::%#] %v"};
  std::shared_ptr<custom_ringbuffer_sink_mt> _ring;
};

void registerLoggingShards() {
  REGISTER_SHARD("Log", Log);
  REGISTER_SHARD("Msg", Msg);
  REGISTER_SHARD("CaptureLog", CaptureLog);
}
}; // namespace shards
