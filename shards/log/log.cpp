#include "log.hpp"
#include "spdlog/fmt/bundled/core.h"
#include <iterator>
#include <spdlog/spdlog.h>
#include <vector>
#include <SDL_stdinc.h>
#include <magic_enum.hpp>
#include <boost/filesystem.hpp>
#include <boost/algorithm/string.hpp>
#include <spdlog/sinks/dist_sink.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#ifdef __ANDROID__
#include <spdlog/sinks/android_sink.h>
#endif

namespace shards::logging {
void init(Logger logger) {
  spdlog::register_logger(logger);
  initLogLevel(logger);
  initLogFormat(logger);
  initSinks(logger);
}

void initLogLevel(Logger logger) {
  std::string loggerName = logger->name();
  std::string varName;

  const char *val{};
  auto tryReadEnvVar = [&]() {
    varName.clear();
    fmt::format_to(std::back_inserter(varName), "LOG_{}", loggerName);
    val = SDL_getenv(varName.c_str());
  };

  tryReadEnvVar();

  if (!val) {
    boost::algorithm::to_lower(loggerName);
    tryReadEnvVar();
  }

  if (!val) {
    varName.clear();
    boost::algorithm::to_upper(loggerName);
    tryReadEnvVar();
  }

  if (val) {
    auto enumVal = magic_enum::enum_cast<spdlog::level::level_enum>(val);
    if (enumVal.has_value()) {
      logger->set_level(enumVal.value());
    }
  }
}

void initLogFormat(Logger logger) {
  std::string varName = fmt::format("LOG_{}_FORMAT", logger->name());
  if (const char *val = SDL_getenv(varName.c_str())) {
    logger->set_pattern(val);
  }
}

void initSinks(Logger logger) { logger->sinks() = spdlog::default_logger()->sinks(); }

Logger get(const std::string &name) {
  auto logger = spdlog::get(name);
  if (!logger) {
    logger = std::make_shared<spdlog::logger>(name);
    init(logger);
    spdlog::register_logger(logger);
  }
  return logger;
}

void redirectAll(const std::vector<spdlog::sink_ptr> &sinks) {
  spdlog::apply_all([&](Logger logger) { logger->sinks() = sinks; });
}

spdlog::level::level_enum getSinkLevel() {
  auto &sinks = spdlog::default_logger()->sinks();
  if (sinks.empty()) {
    return spdlog::level::off;
  }
  return sinks.front()->level();
}

void setSinkLevel(spdlog::level::level_enum level) {
  auto &sinks = spdlog::default_logger()->sinks();
  for (auto &sink : sinks) {
    sink->set_level(level);
  }
}

static void setupDefaultLogger(const std::string &fileName = "shards.log") {
  auto dist_sink = std::make_shared<spdlog::sinks::dist_sink_mt>();

  auto sink1 = std::make_shared<spdlog::sinks::stderr_color_sink_mt>();
  dist_sink->add_sink(sink1);

  // Setup log file
#ifdef SHARDS_LOG_FILE
  std::string logFilePath = boost::filesystem::absolute(fileName).string();
  auto sink2 = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logFilePath.c_str(), true);
  dist_sink->add_sink(sink2);
#endif

  auto logger = std::make_shared<spdlog::logger>("shards", dist_sink);
  logger->flush_on(spdlog::level::err);
  spdlog::set_default_logger(logger);

  // Setup android logcat output
#ifdef __ANDROID__
  auto android_sink = std::make_shared<spdlog::sinks::android_sink_mt>("shards");
  logger->sinks().push_back(android_sink);
#endif

#ifdef __ANDROID
  // Logcat already countains timestamps & log level
  spdlog::set_pattern("[T-%t] [%s::%#] %v");
#else
  spdlog::set_pattern("%^[%l]%$ [%Y-%m-%d %T.%e] [T-%t] [%s::%#] %v");
#endif

  // Set default log level
  logger->set_level(spdlog::level::level_enum(SHARDS_DEFAULT_LOG_LEVEL));

  // Init log level from environment variable
  initLogLevel(logger);
  // Init log format from environment variable
  initLogFormat(logger);

  // Redirect all existing loggers to the default sink
  redirectAll(logger->sinks());
}

void setupDefaultLoggerConditional(std::string fileName) {
  static bool initialized = false;
  if (!initialized) {
    initialized = true;
    setupDefaultLogger(fileName);
  }
}
} // namespace shards::logging
