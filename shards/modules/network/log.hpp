#ifndef BAC295D1_E679_4CB3_B81C_22D4404D79BA
#define BAC295D1_E679_4CB3_B81C_22D4404D79BA

#include <log/log.hpp>

namespace shards::Network {
inline shards::logging::Logger getLogger() {
  return shards::logging::getOrCreate("network", [](shards::logging::Logger logger) {
    // Set default log level to warn to be less verbose
    logger->set_level(spdlog::level::info);
  });
}
} // namespace shards::Network

#endif /* BAC295D1_E679_4CB3_B81C_22D4404D79BA */
