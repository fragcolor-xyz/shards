add_rust_library(NAME shards-markdown
  PROJECT_PATH ${CMAKE_CURRENT_LIST_DIR})

add_shards_module(markdown
  RUST_TARGETS shards-markdown-rust
  REGISTER_SHARDS rust
  EXPERIMENTAL)
