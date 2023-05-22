add_shards_module(csv 
  REGISTER_SHARDS rust
  EXPERIMENTAL)

add_rust_library(NAME shards-csv 
  PROJECT_PATH ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(shards-module-csv shards-csv-rust)
