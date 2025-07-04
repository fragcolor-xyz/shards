#include "log.hpp"
#include <shards/shards.h>
#include <shards/utility.hpp>
#include <spdlog/fmt/bundled/core.h>
#include <shards/core/assert.hpp>
#include <shards/core/platform.hpp>
#include <iterator>
#include <spdlog/spdlog.h>
#include <vector>
#if SHARDS_LOG_SDL
#include <SDL3/SDL_stdinc.h>
#endif
#include <magic_enum.hpp>
#include <shared_mutex>
#include <boost/filesystem.hpp>
#include <boost/algorithm/string.hpp>
#include <spdlog/sinks/dist_sink.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include "process_time.hpp"

#if SH_ANDROID
#include <spdlog/sinks/android_sink.h>
#elif SH_EMSCRIPTEN
#include <emscripten.h>

struct EmscriptenSink : public spdlog::sinks::base_sink<std::mutex> {
  void sink_it_(const spdlog::details::log_msg &msg) override {
    int lv = EM_LOG_CONSOLE;
    switch (msg.level) {
    case spdlog::level::trace:
    case spdlog::level::debug:
      lv |= EM_LOG_DEBUG;
      break;
    case spdlog::level::critical:
    case spdlog::level::err:
      lv |= EM_LOG_ERROR;
      break;
    case spdlog::level::warn:
      lv |= EM_LOG_WARN;
      break;
    default:
    case spdlog::level::info:
      lv |= EM_LOG_INFO;
      break;
    }

    tmpBuffer.assign(msg.payload.data(), msg.payload.size());
    emscripten_log(lv, "%s", tmpBuffer.data());
  }
  void flush_() override {}
  std::string tmpBuffer;
};

#endif

namespace shards::logging {

struct CustomCallbackSink : public spdlog::sinks::base_sink<std::mutex> {
  SHLogCallback callback;
  void *userData;
  CustomCallbackSink(SHLogCallback callback, void *userData) : callback(callback), userData(userData) {}
  void sink_it_(const spdlog::details::log_msg &msg) override {
    callback(SHStringWithLen{msg.logger_name.data(), msg.logger_name.size()}, //
             SHStringWithLen{msg.payload.data(), msg.payload.size()},         //
             msg.level, userData);
  }
  void flush_() override {}
};

std::shared_mutex &__getRegisterMutex() {
  static std::shared_mutex m;
  return m;
}

std::optional<spdlog::level::level_enum> getLogLevelFromEnvVar(std::string inName) {
  std::string varName;
  const char *val{};

#if SHARDS_LOG_SDL
  auto tryReadEnvVar = [&]() {
    varName = inName;
    val = SDL_getenv(varName.c_str());
  };

  tryReadEnvVar();

  if (!val) {
    boost::algorithm::to_lower(inName);
    tryReadEnvVar();
  }

  if (!val) {
    varName.clear();
    boost::algorithm::to_upper(inName);
    tryReadEnvVar();
  }
#endif

  if (val) {
    return magic_enum::enum_cast<spdlog::level::level_enum>(val);
  }
  return std::nullopt;
}

static std::optional<spdlog::level::level_enum> getFlushLogLevel() {
#if SHARDS_LOG_SDL
  auto val = SDL_getenv("LOG_FLUSH_ON");
  if (val) {
    return magic_enum::enum_cast<spdlog::level::level_enum>(val);
  }
#endif
  return std::nullopt;
}

struct Config {
  // -- Compile time settings --
  static constexpr std::optional<spdlog::level::level_enum> DefaultStdOutLogLevel =
#ifdef SHARDS_DEFAULT_STDOUT_LOG_LEVEL
      spdlog::level::level_enum(SHARDS_DEFAULT_STDOUT_LOG_LEVEL);
#else
      std::nullopt;
#endif

  static constexpr std::optional<spdlog::level::level_enum> DefaultFileLogLevel =
#ifdef SHARDS_DEFAULT_FILE_LOG_LEVEL
      spdlog::level::level_enum(SHARDS_DEFAULT_FILE_LOG_LEVEL);
#else
      std::nullopt;
#endif

  static constexpr std::optional<spdlog::level::level_enum> DefaultLoggerLevel =
#ifdef SHARDS_DEFAULT_LOG_LEVEL
      spdlog::level::level_enum(SHARDS_DEFAULT_LOG_LEVEL);
#else
      std::nullopt;
#endif

  static constexpr std::optional<spdlog::level::level_enum> getDefaultLogLevel() {
    if (DefaultLoggerLevel)
      return *DefaultLoggerLevel;

    if (!DefaultStdOutLogLevel && !DefaultFileLogLevel)
      return std::nullopt;

    spdlog::level::level_enum level = spdlog::level::info;
    if (DefaultStdOutLogLevel)
      level = std::min(level, *DefaultStdOutLogLevel);
    if (DefaultFileLogLevel)
      level = std::min(level, *DefaultFileLogLevel);
    return level;
  }
};

thread_local ThreadState threadState{};

struct ShardsSink : public spdlog::sinks::dist_sink_mt {
  void sink_it_(const spdlog::details::log_msg &msg) override {
    auto &threadState_ = threadState;
    std::atomic<LogContext *> pp = threadState_.current;
    while (pp) {
      if (pp.load()->intercept) {
        if (!pp.load()->intercept(msg))
          return;
      }
      pp = pp.load()->prev.load();
    }

    spdlog::sinks::dist_sink_mt::sink_it_(msg);
  }
};

LogContext::LogContext(LogContext &&other) {
  shassert(threadState.current == &other);
  other.pop();
  this->intercept = other.intercept;
  push();
}

void LogContext::linkRootTo(LogContext *other) {
  shassert(prev == nullptr);
  shassert(other != this);
  prev = other;
}

void LogContext::unlink() { prev = nullptr; }

void LogContext::push() {
  prev = threadState.current;
  threadState.current = this;
}

void LogContext::pop() {
  shassert(this == threadState.current);
  threadState.current = prev;
};

struct Sinks {
  std::shared_mutex lock;

  std::shared_ptr<ShardsSink> mainSink;
  std::shared_ptr<spdlog::sinks::stderr_color_sink_mt> stdErrSink;
  std::shared_ptr<spdlog::sinks::sink> logFileSink;
#if SH_ANDROID
  std::shared_ptr<spdlog::sinks::android_sink_mt> androidSink;
#elif SH_EMSCRIPTEN
  std::shared_ptr<EmscriptenSink> emscriptenSink;
#endif

  bool logLevelOverriden{};

  Sinks() {
    mainSink = std::make_shared<ShardsSink>();

#if SH_EMSCRIPTEN
    emscriptenSink = std::make_shared<EmscriptenSink>();
    mainSink->add_sink(emscriptenSink);
#endif

    addStdErrSink();

    // Setup android logcat output
#if SH_ANDROID
    androidSink = std::make_shared<spdlog::sinks::android_sink_mt>("shards");
    mainSink->add_sink(androidSink);
#endif

    if (Config::DefaultStdOutLogLevel) {
      stdErrSink->set_level(Config::DefaultStdOutLogLevel.value());
    }
  }

  void resetStdErrSink() {
    if (auto filter = getLogLevelFromEnvVar("LOG_STDERR_FILTER")) {
      stdErrSink->set_level(*filter);
      logLevelOverriden = true;
    }
  }

  std::unique_lock<std::shared_mutex> lockUnique() { return std::unique_lock<std::shared_mutex>(lock); }
  std::shared_lock<std::shared_mutex> lockShared() { return std::shared_lock<std::shared_mutex>(lock); }

  void initCustomFormatters() {}

  void initStdErrSink() {
#if !SH_EMSCRIPTEN
    stdErrSink = std::make_shared<spdlog::sinks::stderr_color_sink_mt>();
    resetStdErrSink();
#endif
  }

  void addStdErrSink() {
#if !SH_EMSCRIPTEN
    if (stdErrSink)
      mainSink->remove_sink(stdErrSink);
    initStdErrSink();
    mainSink->add_sink(stdErrSink);
#endif
  }

  void initLogFileSink(std::string_view fileName) {
    std::string logFilePath = boost::filesystem::absolute(boost::filesystem::path(std::string(fileName))).string();

#if defined(SHARDS_LOG_ROTATING_MAX_FILE_SIZE) && defined(SHARDS_LOG_ROTATING_MAX_FILES)
    logFileSink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(logFilePath.c_str(), SHARDS_LOG_ROTATING_MAX_FILE_SIZE,
                                                                         SHARDS_LOG_ROTATING_MAX_FILES, true);
#else
    logFileSink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logFilePath.c_str(), true);
#endif

    if (Config::DefaultFileLogLevel) {
      logFileSink->set_level(Config::DefaultFileLogLevel.value());
    }
  }

  // Reset compile-time settings when environment override is detected
  void overrideLogLevel() {
    if (!logLevelOverriden) {
      for (auto &s : mainSink->sinks())
        s->set_level(spdlog::level::trace);
      spdlog::set_level(spdlog::level::info);
      logLevelOverriden = true;
    }
  }
};

Sinks &globalSinks() {
  static Sinks sinks;
  return sinks;
}

void flush() { globalSinks().mainSink->flush(); }

std::shared_ptr<spdlog::sinks::dist_sink_mt> getDistSink() { return globalSinks().mainSink; }

void __init(Logger logger) {
  spdlog::register_logger(logger);
  initFlush(logger);
  initLogLevel(logger);
  initLogFormat(logger);
  initSinks(logger);
}

void initLogLevel(Logger logger) {
  auto level = getLogLevelFromEnvVar(fmt::format("LOG_{}", logger->name()));
  if (level) {
    globalSinks().overrideLogLevel();
    logger->set_level(level.value());
    return;
  }

  // Use "LOG" var to set global log level
  auto globalLevel = getLogLevelFromEnvVar(fmt::format("LOG"));
  if (globalLevel) {
    globalSinks().overrideLogLevel();
    logger->set_level(globalLevel.value());
    return;
  }

  if (auto ll = Config::getDefaultLogLevel()) {
    logger->set_level(ll.value());
  }
}

void initFlush(Logger logger) {
  if (auto flush = getFlushLogLevel()) {
    logger->flush_on(*flush);
  } else {
    logger->flush_on(spdlog::level::err);
  }
}

std::shared_ptr<TimeKeeper> getProcessTimeKeeper() {
  static auto t = std::make_shared<TimeKeeper>();
  return t;
}

void initLogFormat(Logger logger) {
  std::string varName = fmt::format("LOG_{}_FORMAT", logger->name());

  auto formatter = std::make_unique<spdlog::pattern_formatter>();
  formatter->add_flag<ProcessTimeFlag>('P', getProcessTimeKeeper());

#if SHARDS_LOG_SDL
  if (const char *val = SDL_getenv(varName.c_str())) {
    formatter->set_pattern(val);
  } else
#endif
  {
    std::string logPattern;
// Use global log format
#if SHARDS_LOG_SDL
    if (const char *val = SDL_getenv("LOG_FORMAT")) {
      logPattern = val;
    } else
#endif
    {
#ifdef __ANDROID
      // Logcat already countains timestamps & log level
      logPattern = "[T-%t][%n][%s::%#] %v";
#else
      logPattern = "[%d/%m %T.%e][T-%t][%n]%^[%l]%$[%s::%#] %v";
#endif
    }

    formatter->set_pattern(logPattern);
  }

  logger->set_formatter(std::move(formatter));
}

void initSinks(Logger logger) {
  logger->sinks().clear();
  logger->sinks().push_back(globalSinks().mainSink);
}

void initAllSinks() {
  spdlog::apply_all([&](Logger logger) { initSinks(logger); });
}

spdlog::level::level_enum getSinkLevel() { return globalSinks().mainSink->level(); }

void setSinkLevel(spdlog::level::level_enum level) { globalSinks().mainSink->set_level(level); }

void setStdErrLogLevel(spdlog::level::level_enum level) { globalSinks().stdErrSink->set_level(level); }

static bool &logInitialized() {
  static bool initialized = false;
  return initialized;
}

void setupDefaultLogger(const SHLogSettings &settings) {
  auto &sinks = globalSinks();

  std::string_view logFileName;
  if (settings.logToFile) {
    if (settings.logFilePath) {
      logFileName = settings.logFilePath;
    } else {
      logFileName = "shards.log";
    }
  }

  auto l = sinks.lockUnique();

  std::vector<std::shared_ptr<spdlog::sinks::sink>> newSinks;

  if (!logFileName.empty()) {
    sinks.initLogFileSink(logFileName);
    newSinks.push_back(sinks.logFileSink);
  }

  if (settings.logToStdErr) {
    // Reset this sink in case stderr handle changed
    sinks.initStdErrSink();
    newSinks.push_back(sinks.stdErrSink);
  }

  if (settings.callback) {
    newSinks.push_back(std::make_shared<CustomCallbackSink>(settings.callback, settings.callbackUserData));
  }

  // Update all the sinks in one go
  sinks.mainSink->set_sinks(newSinks);

  auto logger = std::make_shared<spdlog::logger>("shards", sinks.mainSink);
  initFlush(logger);
  spdlog::set_default_logger(logger);
  initLogLevel(logger);
  initLogFormat(logger);

  // Sink acts like a filter for all loggers
  setSinkLevel(spdlog::level::trace);

  // Redirect all existing loggers to the global dist sink
  initAllSinks();

  logInitialized() = true;
}

void setupDefaultLoggerConditional(std::string fileName) {
  if (!logInitialized()) {
    setupDefaultLogger(SHLogSettings{
        .logToStdErr = true,
        .logFilePath = fileName.c_str(),
        .logToFile = true,
    });
  }
}

bool isLoggerInitialized() { return logInitialized(); }
} // namespace shards::logging
