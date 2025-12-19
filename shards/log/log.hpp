#ifndef E8296F1D_E25F_4AC4_AA7C_D680CA0D7ABF
#define E8296F1D_E25F_4AC4_AA7C_D680CA0D7ABF

#include <string>

// For freestanding/bare-metal builds, provide stub types
#if SHARDS_NO_SPDLOG

#include <cstdint>
#include <string_view>
#include <memory>

namespace shards::logging {

// Stub log level enum
enum class Level {
  trace = 0,
  debug = 1,
  info = 2,
  warn = 3,
  err = 4,
  critical = 5,
  off = 6,
  n_levels
};

// Stub LogContext
struct LogContext {
  LogContext() {}
  template<typename T> LogContext(T) {}
  ~LogContext() {}
  LogContext(const LogContext &) = delete;
  LogContext &operator=(const LogContext &) = delete;
  LogContext(LogContext &&other) {}
  LogContext &operator=(LogContext &&other) = delete;
  void linkRootTo(LogContext *other) {}
  void unlink() {}
};

struct ThreadState {
  LogContext *current{};
  static ThreadState &get();
};

// Logger is just a null pointer in stub mode
typedef std::nullptr_t Logger;

// Stub options
struct Options {
  const char *name{nullptr};
  bool console{false};
  const char *fileName{nullptr};
};

void init(Options options);
void shutdown();
Logger getOrCreate(std::string_view name, Level level = Level::info);
Level parseLevel(std::string_view str);

// Stub macros - everything is a no-op
#define SPDLOG_TRACE(...) ((void)0)
#define SPDLOG_DEBUG(...) ((void)0)
#define SPDLOG_INFO(...) ((void)0)
#define SPDLOG_WARN(...) ((void)0)
#define SPDLOG_ERROR(...) ((void)0)
#define SPDLOG_CRITICAL(...) ((void)0)
#define SHLOG_TRACE(...) ((void)0)
#define SHLOG_DEBUG(...) ((void)0)
#define SHLOG_INFO(...) ((void)0)
#define SHLOG_WARNING(...) ((void)0)
#define SHLOG_ERROR(...) ((void)0)
#define SHLOG_FATAL(...) ((void)0)

inline bool isLoggerInitialized() { return false; }

} // namespace shards::logging

#else // Full spdlog implementation

#include <spdlog/spdlog.h>
#include <spdlog/sinks/dist_sink.h>
#include <shared_mutex>
#include <vector>

typedef struct SHLogSettings_ SHLogSettings;
namespace shards::logging {

struct ShardsSink;
struct LogContext {
  using CB = bool(const spdlog::details::log_msg &);
  using Cloned = std::vector<std::function<CB>>;
  LogContext() { push(); }
  LogContext(std::function<CB> intercept) : intercept(std::move(intercept)) { push(); }
  ~LogContext() { pop(); }
  LogContext(const LogContext &) = delete;
  LogContext &operator=(const LogContext &) = delete;
  LogContext(LogContext &&other);
  LogContext &operator=(LogContext &&other) = delete;

  // Asuuming this is the root log context, set it's parent
  void linkRootTo(LogContext *other);
  // Unsafe, unlinks this context from it's parent, should only be called on a root context (coro/thread)
  void unlink();

  // Return false to prevent handling the log message
  std::function<CB> intercept;

private:
  void push();
  void pop();
  std::atomic<LogContext *> prev{};
  friend struct ::shards::logging::ShardsSink;
};

struct ThreadState {
  LogContext *current{};
  static ThreadState &get();
};
extern thread_local ThreadState threadState;
inline ThreadState &ThreadState::get() { return threadState; };

typedef std::shared_ptr<spdlog::logger> Logger;
struct TimeKeeper;
std::shared_ptr<TimeKeeper> getProcessTimeKeeper();
// Redirects this logger to the same output as the default logger
void initSinks(Logger logger);
// Sets the log level for this logger based on the LOG_<name> environment variable if it is set
void initLogLevel(Logger logger);
// Init flush_on setting
void initFlush(Logger logger);
// Sets the log format for this logger based on the LOG_<name>_FORMAT environment variable if it is set
void initLogFormat(Logger logger);
void redirectAll(const std::vector<spdlog::sink_ptr> &sinks);

// Controls log filter level of the output sinks
spdlog::level::level_enum getSinkLevel();
void setSinkLevel(spdlog::level::level_enum level);

void setStdErrLogLevel(spdlog::level::level_enum level);

void setupDefaultLogger(const SHLogSettings &settings);

// Setup the default logger if it's not setup already
void setupDefaultLoggerConditional(std::string fileName);

void flush();

std::shared_ptr<spdlog::sinks::dist_sink_mt> getDistSink();

// Set default log level and redirect to main logger
// !! Do not call directly since this registers the logger
// !! use getOrCreate instead to prevent race conditions
void __init(Logger logger);

std::shared_mutex &__getRegisterMutex();
template <typename T> Logger getOrCreate(const std::string &name, T init) {
  auto &m = __getRegisterMutex();
  std::shared_lock<std::shared_mutex> l(m);
  auto logger = spdlog::get(name);
  if (!logger) {
    // Swap to write lock
    l.unlock();
    std::unique_lock<std::shared_mutex> ul(m);

    // Check again after acquiring write lock in case another thread created the logger in between
    logger = spdlog::get(name);
    if (logger)
      return logger;

    logger = std::make_shared<spdlog::logger>(name);
    init(logger);
    __init(logger);
  }
  return logger;
}
inline Logger getOrCreate(const std::string &name) {
  return getOrCreate(name, [](Logger logger) {});
}

bool isLoggerInitialized();

} // namespace shards::logging

#endif // SHARDS_NO_SPDLOG

#endif /* E8296F1D_E25F_4AC4_AA7C_D680CA0D7ABF */
