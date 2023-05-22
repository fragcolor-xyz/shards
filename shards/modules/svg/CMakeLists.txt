add_shards_module(svg
  REGISTER_SHARDS svg
  EXPERIMENTAL)

add_rust_library(NAME shards-svg 
  PROJECT_PATH ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(shards-module-svg shards-svg-rust)
