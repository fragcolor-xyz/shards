add_rust_library(NAME shards-ml
  PROJECT_PATH ${CMAKE_CURRENT_LIST_DIR})

add_shards_module(ml
  REGISTER_SHARDS rust
  RUST_TARGETS shards-ml-rust
  EXPERIMENTAL)
