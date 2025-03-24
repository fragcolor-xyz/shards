#include "log_api.hpp"
#include <spdlog/spdlog.h>
#include <shards/log/log.hpp>
#include <shards/core/assert.hpp>

namespace shards {

struct LoggerKey {
  std::string owned;
  std::string_view view;

  LoggerKey() = default;

  LoggerKey(std::string_view sv) : view(sv) {}

  LoggerKey(SHStringWithLen str) : view(str.string, str.len) {}

  bool operator==(const LoggerKey &other) const { return view == other.view; }

  LoggerKey(const LoggerKey &other) : owned(other.view) { view = owned; }

  LoggerKey(LoggerKey &&other) : owned(std::move(other.owned)) { view = owned; }

  LoggerKey toOwned() {
    LoggerKey n;
    n.owned = std::string(view);
    n.view = n.owned;
    return n;
  }

  struct Hash {
    size_t operator()(const LoggerKey &key) const { return std::hash<std::string_view>{}(key.view); }
  };
};

struct LogBindings {
  std::unordered_map<LoggerKey, std::shared_ptr<spdlog::logger>, LoggerKey::Hash> loggers;

  static LogBindings &instance() {
    static thread_local LogBindings instance;
    return instance;
  }

  std::shared_ptr<spdlog::logger> getLogger(SHStringWithLen cat) {
    auto key = LoggerKey(cat);
    auto it = loggers.find(key);
    if (it != loggers.end()) {
      return it->second;
    }
    auto okey = key.toOwned();
    auto logger = logging::getOrCreate(okey.owned);
    loggers[okey] = logger;
    return logger;
  }
};

void setupCoreLogging(SHCore *result) {
  result->log = [](SHStringWithLen msg) noexcept {
    std::string_view sv(msg.string, size_t(msg.len));
    SHLOG_INFO(sv);
  };

  result->logLevel = [](int level, SHStringWithLen msg) noexcept {
    std::string_view sv(msg.string, size_t(msg.len));
    spdlog::default_logger_raw()->log(spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION}, (spdlog::level::level_enum)level,
                                      sv);
  };

  result->logVar = [](SHStringWithLen cat, int level, SHStringWithLen message, SHVar *var) {
    shassert(var);
    auto logger = LogBindings::instance().getLogger(cat);
    logger->log(static_cast<spdlog::level::level_enum>(level), "{}: {}", message, *var);
  };

  result->logLogger = [](SHStringWithLen cat, int level, SHStringWithLen message) {
    auto logger = LogBindings::instance().getLogger(cat);
    logger->log(static_cast<spdlog::level::level_enum>(level), "{}", message);
  };
}
} // namespace shards