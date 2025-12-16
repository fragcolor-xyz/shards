add_rust_library(NAME shards-geo
  PROJECT_PATH ${CMAKE_CURRENT_LIST_DIR})

add_shards_module(geo
  REGISTER_SHARDS geo
  RUST_TARGETS shards-geo-rust)
