add_rust_library(NAME shards-svg
  PROJECT_PATH ${CMAKE_CURRENT_LIST_DIR})

add_shards_module(svg
  REGISTER_SHARDS svg
  RUST_TARGETS shards-svg-rust
  EXPERIMENTAL)
