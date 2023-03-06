#ifndef DDE8DFC5_0F92_481A_AF98_812B4A76BE44
#define DDE8DFC5_0F92_481A_AF98_812B4A76BE44

#include "spdlog/common.h"
#include <log/log.hpp>
namespace gfx {

inline shards::logging::Logger getLogger() {
  auto logger = spdlog::get("GFX");
  if (!logger) {
    logger = std::make_shared<spdlog::logger>("GFX");
    // Set default log level to warn to be less verbose
    logger->set_level(spdlog::level::warn);
    shards::logging::init(logger);
  }
  return logger;
}

inline shards::logging::Logger getWgpuLogger() {
  auto logger = spdlog::get("wgpu");
  if (!logger) {
    logger = std::make_shared<spdlog::logger>("wgpu");
    logger->set_level(spdlog::level::err);
    shards::logging::init(logger);
  }
  return logger;
}
} // namespace gfx

#endif /* DDE8DFC5_0F92_481A_AF98_812B4A76BE44 */
