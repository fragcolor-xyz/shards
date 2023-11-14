#ifndef C2052FE4_794D_41AC_8A5F_DE82A84C764C
#define C2052FE4_794D_41AC_8A5F_DE82A84C764C

#include <log/log.hpp>

namespace shards::input { 
inline shards::logging::Logger getLogger() {
  auto logger = spdlog::get("input");
  if (!logger) {
    logger = std::make_shared<spdlog::logger>("input");
    // Set default log level to warn to be less verbose
    logger->set_level(spdlog::level::info);
    shards::logging::init(logger);
  }
  return logger;
}
}

#endif /* C2052FE4_794D_41AC_8A5F_DE82A84C764C */
