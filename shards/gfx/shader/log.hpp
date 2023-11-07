#ifndef DA5BB6B8_41E8_4742_B134_6E275652F2F2
#define DA5BB6B8_41E8_4742_B134_6E275652F2F2

#include <log/log.hpp>
namespace gfx::shader {
inline shards::logging::Logger getLogger() {
  auto logger = spdlog::get("shader");
  if (!logger) {
    logger = std::make_shared<spdlog::logger>("shader");
    // Set default log level to warn to be less verbose
    logger->set_level(spdlog::level::warn);
    shards::logging::init(logger);
  }
  return logger;
}
} // namespace gfx::shader

#endif /* DA5BB6B8_41E8_4742_B134_6E275652F2F2 */
