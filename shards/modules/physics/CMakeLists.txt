add_shards_module(physics 
  REGISTER_SHARDS physics)

add_rust_library(NAME shards-physics 
  PROJECT_PATH ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(shards-module-physics shards-physics-rust)
