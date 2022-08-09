#ifndef E8296F1D_E25F_4AC4_AA7C_D680CA0D7ABF
#define E8296F1D_E25F_4AC4_AA7C_D680CA0D7ABF

#include <string>
#include <spdlog/spdlog.h>
#include <vector>

namespace shards::logging {
typedef std::shared_ptr<spdlog::logger> Logger;
Logger get(const std::string &name);
// Set default log level and redirect to main logger
void init(Logger logger);
// Redirects this logger to the same output as the default logger
void initSinks(Logger logger);
// Sets the log level for this logger based on the LOG_<name> environment variable if it is set
void initLogLevel(Logger logger);
void redirectAll(const std::vector<spdlog::sink_ptr> &sinks);

// Setup the default logger if it's not setup already
void setupDefaultLoggerConditional();
} // namespace shards::logging

#endif /* E8296F1D_E25F_4AC4_AA7C_D680CA0D7ABF */
