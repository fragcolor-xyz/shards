add_library(shards-logging log.cpp)
target_include_directories(shards-logging PUBLIC ${CMAKE_CURRENT_LIST_DIR}/..)
target_link_libraries(shards-logging spdlog SDL2 magic_enum Boost::filesystem Boost::algorithm)

if(DESKTOP)
  target_compile_definitions(shards-logging PRIVATE SHARDS_LOG_FILE=1)
endif()

# The compile-time filtered log level
if(NOT SPDLOG_ACTIVE_LEVEL)
  if((CMAKE_BUILD_TYPE STREQUAL "Release") OR(CMAKE_BUILD_TYPE STREQUAL "MinSizeRel"))
    set(SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_INFO)
  else()
    set(SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE)
  endif()
endif()

# The default enabled log level
if(NOT SHARDS_DEFAULT_LOG_LEVEL)
  if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(SHARDS_DEFAULT_LOG_LEVEL SPDLOG_LEVEL_INFO)
  else()
    set(SHARDS_DEFAULT_LOG_LEVEL SPDLOG_LEVEL_TRACE)
  endif()
endif()

target_compile_definitions(shards-logging
  PUBLIC SPDLOG_ACTIVE_LEVEL=${SPDLOG_ACTIVE_LEVEL}
  PRIVATE
  SHARDS_DEFAULT_LOG_LEVEL=${SHARDS_DEFAULT_LOG_LEVEL})

if(SHARDS_DEFAULT_STDOUT_LOG_LEVEL)
  target_compile_definitions(shards-logging PRIVATE SHARDS_DEFAULT_STDOUT_LOG_LEVEL=${SHARDS_DEFAULT_STDOUT_LOG_LEVEL})
endif()

if(SHARDS_DEFAULT_FILE_LOG_LEVEL)
  target_compile_definitions(shards-logging PRIVATE SHARDS_DEFAULT_FILE_LOG_LEVEL=${SHARDS_DEFAULT_FILE_LOG_LEVEL})
endif()