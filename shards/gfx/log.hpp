#ifndef DDE8DFC5_0F92_481A_AF98_812B4A76BE44
#define DDE8DFC5_0F92_481A_AF98_812B4A76BE44

#include "spdlog/common.h"
#include <log/log.hpp>
namespace gfx {

inline shards::logging::Logger getLogger() {
  return shards::logging::getOrCreate("gfx", [](shards::logging::Logger logger) {
    // Set default log level to warn to be less verbose
    logger->set_level(spdlog::level::warn);
  });
}

inline shards::logging::Logger getWgpuLogger() {
  return shards::logging::getOrCreate("wgpu", [](shards::logging::Logger logger) {
    logger->set_level(spdlog::level::err);
  });
}
} // namespace gfx

#endif /* DDE8DFC5_0F92_481A_AF98_812B4A76BE44 */
