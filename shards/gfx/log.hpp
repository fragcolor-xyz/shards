#ifndef DDE8DFC5_0F92_481A_AF98_812B4A76BE44
#define DDE8DFC5_0F92_481A_AF98_812B4A76BE44

#include "spdlog/common.h"
#include <shards/log/log.hpp>
namespace gfx {

inline shards::logging::Logger getLogger() {
  return shards::logging::getOrCreate("gfx");
}

inline shards::logging::Logger getWgpuLogger() {
  return shards::logging::getOrCreate("wgpu");
}
} // namespace gfx

#endif /* DDE8DFC5_0F92_481A_AF98_812B4A76BE44 */
