add_rust_library(NAME shards-pdf
  PROJECT_PATH ${CMAKE_CURRENT_LIST_DIR})

add_shards_module(pdf
  RUST_TARGETS shards-pdf-rust
  REGISTER_SHARDS rust
  EXPERIMENTAL)
