set(SOURCES
  fs.cpp
)

add_shards_module(fs SOURCES ${SOURCES}
  REGISTER_SHARDS fs rust)

target_link_libraries(shards-module-fs Boost::filesystem)

add_rust_library(NAME shards-fs 
  PROJECT_PATH ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(shards-module-fs shards-fs-rust)
