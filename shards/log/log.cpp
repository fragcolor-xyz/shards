#include "log.hpp"
#include "spdlog/fmt/bundled/core.h"
#include <iterator>
#include <spdlog/spdlog.h>
#include <vector>
#include <SDL_stdinc.h>
#include <magic_enum.hpp>
#include <shared_mutex>
#include <boost/filesystem.hpp>
#include <boost/algorithm/string.hpp>
#include <spdlog/sinks/dist_sink.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#ifdef __ANDROID__
#include <spdlog/sinks/android_sink.h>
#endif

namespace shards::logging {

std::shared_mutex &__getRegisterMutex() {
  static std::shared_mutex m;
  return m;
}

std::optional<spdlog::level::level_enum> getLogLevelFromEnvVar(std::string inName) {
  std::string varName;
  const char *val{};
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

  if (val) {
    return magic_enum::enum_cast<spdlog::level::level_enum>(val);
  }
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

struct Sinks {
  std::shared_mutex lock;

  std::shared_ptr<spdlog::sinks::dist_sink_mt> distSink;
  std::shared_ptr<spdlog::sinks::stderr_color_sink_mt> stdErrSink;
  std::shared_ptr<spdlog::sinks::basic_file_sink_mt> logFileSink;
#ifdef __ANDROID__
  std::shared_ptr<spdlog::sinks::android_sink_mt> androidSink;
#endif

  bool logLevelOverriden{};

  Sinks() {
    distSink = std::make_shared<spdlog::sinks::dist_sink_mt>();

    initStdErrSink();

    // Setup android logcat output
#ifdef __ANDROID__
    androidSink = std::make_shared<spdlog::sinks::android_sink_mt>("shards");
    distSink->add_sink(androidSink);
#endif

    if (Config::DefaultStdOutLogLevel) {
      stdErrSink->set_level(Config::DefaultStdOutLogLevel.value());
    }
    if (auto filter = getLogLevelFromEnvVar("LOG_STDERR_FILTER")) {
      stdErrSink->set_level(*filter);
      logLevelOverriden = true;
    }
  }

  std::unique_lock<std::shared_mutex> lockUnique() { return std::unique_lock<std::shared_mutex>(lock); }
  std::shared_lock<std::shared_mutex> lockShared() { return std::shared_lock<std::shared_mutex>(lock); }

  void initStdErrSink() {
    if (stdErrSink)
      distSink->remove_sink(stdErrSink);
    stdErrSink = std::make_shared<spdlog::sinks::stderr_color_sink_mt>();
    distSink->add_sink(stdErrSink);
  }

  void initLogFile(std::string fileName) {
    std::string logFilePath = boost::filesystem::absolute(fileName).string();
    if (logFileSink)
      distSink->remove_sink(logFileSink);

    logFileSink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logFilePath.c_str(), true);
    if (Config::DefaultFileLogLevel) {
      logFileSink->set_level(Config::DefaultFileLogLevel.value());
    }
    distSink->add_sink(logFileSink);
  }

  // Reset compile-time settings when environment override is detected
  void overrideLogLevel() {
    if (!logLevelOverriden) {
      for (auto &s : distSink->sinks())
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

void __init(Logger logger) {
  spdlog::register_logger(logger);
  logger->flush_on(spdlog::level::err);
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

void initLogFormat(Logger logger) {
  std::string varName = fmt::format("LOG_{}_FORMAT", logger->name());
  if (const char *val = SDL_getenv(varName.c_str())) {
    logger->set_pattern(val);
  } else {
    std::string logPattern;
    // Use global log format
    if (const char *val = SDL_getenv("LOG_FORMAT")) {
      logPattern = val;
    } else {
#ifdef __ANDROID
      // Logcat already countains timestamps & log level
      logPattern = "[T-%t][%n][%s::%#] %v";
#else
      logPattern = "[%d/%m %T.%e][T-%t][%n]%^[%l]%$[%s::%#] %v";
#endif
    }

    logger->set_pattern(logPattern);
  }
}

void initSinks(Logger logger) {
  logger->sinks().clear();
  logger->sinks().push_back(globalSinks().distSink);
}

void initAllSinks() {
  spdlog::apply_all([&](Logger logger) { initSinks(logger); });
}

spdlog::level::level_enum getSinkLevel() { return globalSinks().distSink->level(); }

void setSinkLevel(spdlog::level::level_enum level) { globalSinks().distSink->set_level(level); }

static void setupDefaultLogger(const std::string &fileName = "shards.log") {
  auto &sinks = globalSinks();

  {
    auto l = sinks.lockUnique();
    sinks.initLogFile(fileName);
    // Reset this sink in case stderr handle changed
    sinks.initStdErrSink();
  }

  auto logger = std::make_shared<spdlog::logger>("shards", sinks.distSink);
  logger->flush_on(spdlog::level::err);
  spdlog::set_default_logger(logger);
  initLogLevel(logger);
  initLogFormat(logger);

  // Sink acts like a filter for all loggers
  setSinkLevel(spdlog::level::trace);

  // Redirect all existing loggers to the global dist sink
  initAllSinks();
}

void setupDefaultLoggerConditional(std::string fileName) {
  static bool initialized = false;
  if (!initialized) {
    initialized = true;
    setupDefaultLogger(fileName);
  }
}
} // namespace shards::logging
