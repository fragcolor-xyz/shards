#ifndef C2052FE4_794D_41AC_8A5F_DE82A84C764C
#define C2052FE4_794D_41AC_8A5F_DE82A84C764C

#include <log/log.hpp>

namespace shards::input { 
inline shards::logging::Logger getLogger() {
  return shards::logging::getOrCreate("input", [](shards::logging::Logger logger) {
    // Set default log level to warn to be less verbose
    logger->set_level(spdlog::level::info);
  });
}
}

#endif /* C2052FE4_794D_41AC_8A5F_DE82A84C764C */
