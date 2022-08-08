add_library(shards-logging log.cpp)
target_include_directories(shards-logging PUBLIC ${CMAKE_CURRENT_LIST_DIR}/..)
target_link_libraries(shards-logging spdlog SDL2-static magic_enum Boost::filesystem)

if(DESKTOP)
  target_compile_definitions(shards-logging PRIVATE SHARDS_LOG_FILE=1)
endif()
