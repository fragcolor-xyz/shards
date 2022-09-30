/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2019 Fragcolor Pte. Ltd. */

#include "shared.hpp"
#include <numeric>
#include <string>
#include <log/log.hpp>
#include <spdlog/sinks/base_sink.h>
#include <spdlog/sinks/dist_sink.h>
// #include <spdlog/sinks/ringbuffer_sink.h>

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
template<typename Mutex>
class custom_ringbuffer_sink final : public spdlog::sinks::base_sink<Mutex>
{
public:
    explicit custom_ringbuffer_sink(size_t n_items)
        : q_{n_items}
    {}

    std::vector<spdlog::details::log_msg_buffer> last_raw(size_t lim = 0)
    {
        std::lock_guard<Mutex> lock(spdlog::sinks::base_sink<Mutex>::mutex_);
        auto items_available = q_.size();
        auto n_items = lim > 0 ? (std::min)(lim, items_available) : items_available;
        std::vector<spdlog::details::log_msg_buffer> ret;
        ret.reserve(n_items);
        for (size_t i = (items_available - n_items); i < items_available; i++)
        {
            ret.push_back(q_.at(i));
        }
        return ret;
    }

    std::vector<std::string> last_formatted(size_t lim = 0)
    {
        std::lock_guard<Mutex> lock(spdlog::sinks::base_sink<Mutex>::mutex_);
        auto items_available = q_.size();
        auto n_items = lim > 0 ? (std::min)(lim, items_available) : items_available;
        std::vector<std::string> ret;
        ret.reserve(n_items);
        for (size_t i = (items_available - n_items); i < items_available; i++)
        {
            spdlog::memory_buf_t formatted;
            spdlog::sinks::base_sink<Mutex>::formatter_->format(q_.at(i), formatted);
            ret.push_back(fmt::to_string(formatted));
        }
        return ret;
    }

protected:
    void sink_it_(const spdlog::details::log_msg &msg) override
    {
        q_.push_back(spdlog::details::log_msg_buffer{msg});
    }
    void flush_() override {}

private:
    spdlog::details::circular_q<spdlog::details::log_msg_buffer> q_;
};

using custom_ringbuffer_sink_mt = custom_ringbuffer_sink<std::mutex>;
using custom_ringbuffer_sink_st = custom_ringbuffer_sink<spdlog::details::null_mutex>;

struct CaptureLog {
  static SHTypesInfo inputTypes() { return CoreInfo::NoneType; }

  static SHTypesInfo outputTypes() {
    // FIXME
    return CoreInfo::StringType;
    // return CoreInfo::StringSeqType;
  }

  void warmup(SHContext *context) {
    auto logger = spdlog::default_logger();
    assert(logger);
    auto sink = logger->sinks().at(0);
    assert(sink);
    auto dist_sink = std::dynamic_pointer_cast<spdlog::sinks::dist_sink_mt>(sink);
    if (dist_sink) {
      _ring = std::make_shared<custom_ringbuffer_sink_mt>(8);
#ifdef __ANDROID
      // Logcat already countains timestamps & log level
      _ring->set_formatter(std::unique_ptr<spdlog::formatter>(
          new spdlog::pattern_formatter(std::move("[T-%t] [%s::%#] %v"), spdlog::pattern_time_type::local)));
#else
      _ring->set_formatter(std::unique_ptr<spdlog::formatter>(new spdlog::pattern_formatter(
          std::move("%^[%l]%$ [%Y-%m-%d %T.%e] %v"), spdlog::pattern_time_type::local)));
#endif
      dist_sink->add_sink(_ring);
    }
  }

  void cleanup() {
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
    if (_ring) {
      auto msgs = _ring->last_formatted();

      _s.clear();
      for (auto i = msgs.begin(); i != msgs.end(); ++i) {
        _s += *i;
      }
      return Var(_s);
    }
    return Var("");
  }

private:
  std::shared_ptr<custom_ringbuffer_sink_mt> _ring;
  std::string _s{};
};

void registerLoggingShards() {
  REGISTER_SHARD("Log", Log);
  REGISTER_SHARD("Msg", Msg);
  REGISTER_SHARD("CaptureLog", CaptureLog);
}
}; // namespace shards
