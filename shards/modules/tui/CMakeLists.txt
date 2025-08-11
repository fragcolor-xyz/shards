add_rust_library(NAME shards-tui
  PROJECT_PATH ${CMAKE_CURRENT_LIST_DIR})

add_shards_module(tui
  RUST_TARGETS shards-tui-rust
  REGISTER_SHARDS rust
  EXPERIMENTAL)
