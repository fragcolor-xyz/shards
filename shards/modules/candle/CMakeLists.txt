add_rust_library(NAME shards-candle
  PROJECT_PATH ${CMAKE_CURRENT_LIST_DIR})

add_shards_module(candle
  REGISTER_SHARDS rust
  RUST_TARGETS shards-candle-rust
  EXPERIMENTAL)
