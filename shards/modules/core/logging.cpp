/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2019 Fragcolor Pte. Ltd. */

#include "logging.hpp"
#include <shards/shardwrapper.hpp>
#include <shards/utility.hpp>
#include <shards/core/params.hpp>
#include <shards/log/process_time.hpp>
#include <spdlog/spdlog.h>
#include <atomic>
#include <numeric>
#include <string>
#include <cstdio>
#include <shards/log/log.hpp>
#include <spdlog/sinks/dist_sink.h>
#include <oneapi/tbb/concurrent_queue.h>

namespace shards {

struct LoggingBase {
  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return CoreInfo::AnyType; }

  PARAM_PARAMVAR(_level, "Level", "The logging level to use.",
                 {Enums::LogLevelEnumInfo::Type, Type::VariableOf(Enums::LogLevelEnumInfo::Type)})
  PARAM_PARAMVAR(_name, "Name", "The name of the logger to use.", {CoreInfo::StringType, Type::VariableOf(CoreInfo::StringType)})
  PARAM_IMPL(PARAM_IMPL_FOR(_level), PARAM_IMPL_FOR(_name))

  LoggingBase() {
    _name = Var("shards");
    _level = Var::Enum(Enums::LogLevel::Info, Enums::LogLevelEnumInfo::Type);
  }

  void warmup(SHContext *context) { _logger = shards::logging::getOrCreate(SHSTRING_PREFER_SHSTRVIEW(_name.get())); }

  void maybeUpdateDynamicLogger() {
    if (_name.isVariable() && _logger->name() != SHSTRVIEW(_name.get())) {
      _logger = shards::logging::getOrCreate(SHSTRING_PREFER_SHSTRVIEW(_name.get()));
    }
  }

protected:
  shards::logging::Logger _logger;
};

#define SHLOG_LEVEL(_level_, ...)                                                                      \
  {                                                                                                    \
    SPDLOG_LOGGER_CALL(spdlog::default_logger_raw(), spdlog::level::level_enum(_level_), __VA_ARGS__); \
  }

struct Log : public LoggingBase {
  static SHOptionalString inputHelp() { return SHCCSTR("The value to be logged to the console."); }

  static SHOptionalString outputHelp() { return SHCCSTR("The same value that was inputted, unmodified."); }

  static SHOptionalString help() {
    return SHCCSTR("Logs the output of a shard or the value of a variable to the console along with an optional prefix string "
                   "(note that the system will add a `:` to the prefix). "
                   "The logging level can be specified to control the verbosity of the log output.");
  }

  PARAM_PARAMVAR(_prefix, "Prefix", "A prefix string to be added to the log message.", {CoreInfo::StringType})
  PARAM_IMPL_DERIVED_PREPEND(LoggingBase, PARAM_IMPL_FOR(_prefix))

  void warmup(SHContext *context) {
    PARAM_WARMUP(context);
    LoggingBase::warmup(context);
  }

  void cleanup(SHContext *context) { PARAM_CLEANUP(context); }

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    return data.inputType;
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    maybeUpdateDynamicLogger();
    auto current = context->wireStack.back();
    auto id = findId(context);
    auto prefix = _prefix.get();
    auto level = spdlog::level::level_enum(_level.get().payload.intValue);
    auto prefixSV = fmt::basic_string_view<char>(prefix.payload.stringValue, prefix.payload.stringLen);
    if (prefixSV.size() > 0) {
      if (id != entt::null) {
        SPDLOG_LOGGER_CALL(_logger, level, "[{} {}] {}: {}", current->name, id, prefixSV, input);
      } else {
        SPDLOG_LOGGER_CALL(_logger, level, "[{}] {}: {}", current->name, prefixSV, input);
      }
    } else {
      if (id != entt::null) {
        SPDLOG_LOGGER_CALL(_logger, level, "[{} {}] {}", current->name, id, input);
      } else {
        SPDLOG_LOGGER_CALL(_logger, level, "[{}] {}", current->name, input);
      }
    }
    return input;
  }
};

struct LogType : public Log {
  static SHOptionalString inputHelp() { return SHCCSTR("The value whose type will be logged to the console."); }

  static SHOptionalString outputHelp() { return SHCCSTR("The same value that was inputted, unmodified."); }

  static SHOptionalString help() {
    return SHCCSTR("Logs the type of the value to the console along with an optional prefix string. The logging level can be "
                   "specified to control the verbosity of the log output.");
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    maybeUpdateDynamicLogger();
    auto current = context->wireStack.back();
    auto id = findId(context);
    auto prefix = _prefix.get();
    auto level = spdlog::level::level_enum(_level.get().payload.intValue);
    auto prefixSV = fmt::basic_string_view<char>(prefix.payload.stringValue, prefix.payload.stringLen);
    if (prefixSV.size() > 0) {
      if (id != entt::null) {
        SPDLOG_LOGGER_CALL(_logger, level, "[{} {}] {}: {}", current->name, id, prefixSV, type2Name(input.valueType));
      } else {
        SPDLOG_LOGGER_CALL(_logger, level, "[{}] {}: {}", current->name, prefixSV, type2Name(input.valueType));
      }
    } else {
      if (id != entt::null) {
        SPDLOG_LOGGER_CALL(_logger, level, "[{} {}] {}", current->name, id, type2Name(input.valueType));
      } else {
        SPDLOG_LOGGER_CALL(_logger, level, "[{}] {}", current->name, type2Name(input.valueType));
      }
    }
    return input;
  }
};

struct Msg : public LoggingBase {
  static SHOptionalString inputHelp() { return SHCCSTR("The input is ignored. This shard displays a static message."); }

  static SHOptionalString outputHelp() { return SHCCSTR("The same variable that was inputted, unmodified."); }

  static SHOptionalString help() {
    return SHCCSTR("Displays the passed message string to the user via standard output. The input variable is ignored, and only "
                   "the static message is displayed.");
  }

  PARAM_PARAMVAR(_msg, "Message", "The message to display on the user's screen or console.",
                 {CoreInfo::StringType, Type::VariableOf(CoreInfo::StringType)})
  PARAM_VAR(_raw, "Raw", "Ignore all other formatting and output the message as-is.", {CoreInfo::BoolType})
  PARAM_IMPL_DERIVED_PREPEND(LoggingBase, PARAM_IMPL_FOR(_msg), PARAM_IMPL_FOR(_raw))

  void warmup(SHContext *context) {
    PARAM_WARMUP(context);
    LoggingBase::warmup(context);
  }

  void cleanup(SHContext *context) { PARAM_CLEANUP(context); }

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    return data.inputType;
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    maybeUpdateDynamicLogger();
    auto level = spdlog::level::level_enum(_level.get().payload.intValue);
    auto msgSV = fmt::basic_string_view<char>(_msg.get().payload.stringValue, _msg.get().payload.stringLen);
    if (_raw.payload.boolValue) {
      SPDLOG_LOGGER_CALL(_logger, level, msgSV);
    } else {
      auto current = context->wireStack.back();
      auto id = findId(context);
      if (id != entt::null) {
        SPDLOG_LOGGER_CALL(_logger, level, "[{} {}] {}", current->name, id, msgSV);
      } else {
        SPDLOG_LOGGER_CALL(_logger, level, "[{}] {}", current->name, msgSV);
      }
    }
    return input;
  }
};

SHVar logsFlushActivation(const SHVar &input) {
  spdlog::default_logger()->flush();
  return input;
}

SHVar logsChangeLevelActivation(const SHVar &input) {
  auto level = SHSTRING_PREFER_SHSTRVIEW(input);
  logging::setSinkLevel(spdlog::level::from_str(level));
  return input;
}

struct LogCaptureContext {
  static inline const char VariableName[] = "Logging.CaptureBuffer";
  static constexpr uint32_t TypeId = 'LcPT';
  static inline SHTypeInfo Type{SHType::Object, {.object = {.vendorId = CoreCC, .typeId = TypeId}}};
  static inline const SHOptionalString VariableDescription = SHCCSTR("The log capture context.");
  static inline SHExposedTypeInfo VariableInfo = shards::ExposedInfo::ProtectedVariable(VariableName, VariableDescription, Type);

  oneapi::tbb::concurrent_queue<spdlog::memory_buf_t> _messages;
  std::vector<spdlog::memory_buf_t> _stringBuffer;

  void drain() {
    // Now flush the queue into the output sequence
    auto size = _messages.unsafe_size();
    auto ofs = _stringBuffer.size();
    _stringBuffer.resize(ofs + size);
    for (size_t i = 0; i < size; i++) {
      spdlog::memory_buf_t &msg = _stringBuffer[ofs + i];
      if (!_messages.try_pop(msg)) {
        _stringBuffer.resize(ofs + i);
        break;
      }
    }
  }

  static void stringBufferInto(std::vector<spdlog::memory_buf_t> &stringBuffer, SeqVar &sv) {
    sv.resize(stringBuffer.size());
    for (size_t i = 0; i < stringBuffer.size(); i++) {
      spdlog::memory_buf_t &msg = stringBuffer[i];
      sv[i] = OwnedVar::Foreign(std::string_view(msg.data(), msg.size()));
    }
  }
};
typedef shards::RequiredContextVariable<LogCaptureContext, LogCaptureContext::Type, LogCaptureContext::VariableName>
    RequiredLogCaptureContext;

struct CaptureLog {
  static SHTypesInfo inputTypes() { return CoreInfo::NoneType; }
  static SHTypesInfo outputTypes() { return CoreInfo::StringSeqType; }

  static SHOptionalString help() {
    return SHCCSTR("Captures log messages with additional control over silent mode and format pattern. "
                   "When silent mode is enabled, log output is suppressed. "
                   "A custom format pattern can be specified to control how log messages are formatted.");
  }

  PARAM(ShardsVar, _content, "Content", "The content of the log message", {CoreInfo::Shards})
  PARAM_VAR(_silent, "Silent", "Whether to suppress log output", {CoreInfo::BoolType})
  PARAM_VAR(
      _format, "Format",
      "Custom format pattern for log messages (%P will record the process's life time, %p will record the shard's life time)",
      {CoreInfo::StringType})
  PARAM_VAR(_minLevel, "MinLevel", "The minimum level of logs to capture", {Enums::LogLevelEnumInfo::Type})
  PARAM_IMPL(PARAM_IMPL_FOR(_content), PARAM_IMPL_FOR(_silent), PARAM_IMPL_FOR(_format), PARAM_IMPL_FOR(_minLevel));

  SeqVar _seqView;
  bool _passThrough{};
  const char *_pattern{};
  std::shared_ptr<logging::TimeKeeper> _timeKeeper;
  std::unique_ptr<spdlog::pattern_formatter> _formatter;

  LogCaptureContext _ctx;
  ParamVar _captureContext;

  CaptureLog() {
    // Default format
    _format = Var("[%l] %v");
    _silent = Var(false);
    _timeKeeper = std::make_shared<logging::TimeKeeper>();
    _formatter = std::make_unique<spdlog::pattern_formatter>("%v", spdlog::pattern_time_type::local, "");
    _formatter->add_flag<logging::ProcessTimeFlag>('P', logging::getProcessTimeKeeper());
    _formatter->add_flag<logging::ProcessTimeFlag>('p', _timeKeeper);
    _minLevel = Var::Enum(Enums::LogLevel::Info, Enums::LogLevelEnumInfo::Type);
    _captureContext = Var::ContextVar(LogCaptureContext::VariableName);
  }

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    ExposedInfo inner(data.shared);
    inner.push_back(RequiredLogCaptureContext::getExposedTypeInfo());
    SHInstanceData dataInner = data;
    dataInner.shared = SHExposedTypesInfo(inner);
    _content.compose(dataInner);
    return outputTypes().elements[0];
  }

  logging::LogContext createScopedLogContext() {
    bool passThrough = !_silent.payload.boolValue;
    auto minLevel = spdlog::level::level_enum(_minLevel.payload.enumValue);
    return logging::LogContext([this, passThrough, minLevel](const spdlog::details::log_msg &msg) {
      if (int(msg.level) < int(minLevel)) {
        return passThrough;
      }
      spdlog::memory_buf_t mb;
      _formatter->format(msg, mb);
      _ctx._messages.emplace(std::move(mb));
      return passThrough;
    });
  }

  SHExposedTypesInfo exposedVariables() { return _content.composeResult().exposedInfo; }

  void warmup(SHContext *context) {
    _formatter->set_pattern(_format.payload.stringValue);
    auto $ = createScopedLogContext();
    // Warmup after so we can collect warmup logs
    _captureContext.warmup(context);
    assignVariableValue(_captureContext.get(), Var::Object(&_ctx, LogCaptureContext::Type));
    PARAM_WARMUP(context);
  }

  void cleanup(SHContext *context) {
    PARAM_CLEANUP(context);
    _captureContext.cleanup(context);
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    {
      auto $ = createScopedLogContext();
      SHVar out{};
      _content.activate(context, input, out);
    }

    // Now flush the queue into the output sequence
    _ctx.drain();
    _seqView.clear();
    LogCaptureContext::stringBufferInto(_ctx._stringBuffer, _seqView);
    return _seqView;
  }
};

struct CurrentCaptureLog {
  RequiredLogCaptureContext _captureContext;

  SeqVar _output;
  std::vector<std::string> _tmpBuffer;

  SHTypesInfo inputTypes() { return CoreInfo::NoneType; }
  SHTypesInfo outputTypes() { return CoreInfo::StringSeqType; }

  PARAM_IMPL();
  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    _captureContext.compose(data, _requiredVariables);
    return CoreInfo::StringSeqType;
  }
  void warmup(SHContext *context) {
    PARAM_WARMUP(context);
    _captureContext.warmup(context);
  }
  void cleanup(SHContext *context) {
    PARAM_CLEANUP(context);
    _output.clear();
    _tmpBuffer.clear();
    _captureContext.cleanup();
  }
  SHVar activate(SHContext *context, const SHVar &input) {
    _captureContext.get()->drain();
    auto &src = _captureContext.get()->_stringBuffer;
    _tmpBuffer.resize(src.size());
    _output.resize(src.size());
    for (size_t i = 0; i < src.size(); i++) {
      spdlog::memory_buf_t &msg = src[i];
      auto &dst = _tmpBuffer[i];
      dst = std::string(msg.data(), msg.size());
      _output[i] = OwnedVar::Foreign(std::string_view(dst.data(), dst.size()));
    }
    return _output;
  }
};

struct LogFlush : public LambdaShard<logsFlushActivation, CoreInfo::AnyType, CoreInfo::AnyType> {
  static SHOptionalString help() {
    return SHCCSTR("This shard flushes the log buffer to the console. This ensures that any pending log messages are "
                   "immediately written to the console.");
  }

  static SHOptionalString inputHelp() { return DefaultHelpText::InputHelpPass; }

  static SHOptionalString outputHelp() { return DefaultHelpText::OutputHelpPass; }
};

struct LogChangeLevel : public LambdaShard<logsChangeLevelActivation, CoreInfo::StringType, CoreInfo::AnyType> {
  static SHOptionalString help() {
    return SHCCSTR("This shard changes the log level to the level specified by the string passed as input. ");
  }

  static SHOptionalString inputHelp() {
    return SHCCSTR("A string representing the new log level (e.g., 'debug', 'info', 'warn', 'error', 'critical').");
  }

  static SHOptionalString outputHelp() { return DefaultHelpText::OutputHelpPass; }
};

SHARDS_REGISTER_FN(logging) {
  REGISTER_SHARD("Log", Log);
  REGISTER_SHARD("LogType", LogType);
  REGISTER_SHARD("Msg", Msg);
  REGISTER_SHARD("CaptureLog", CaptureLog);
  REGISTER_SHARD("CurrentCaptureLog", CurrentCaptureLog);
  REGISTER_SHARD("FlushLog", LogFlush);
  REGISTER_SHARD("SetLogLevel", LogChangeLevel);
  REGISTER_ENUM(Enums::LogLevelEnumInfo);
}
}; // namespace shards
