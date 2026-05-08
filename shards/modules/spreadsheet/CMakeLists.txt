add_rust_library(NAME shards-spreadsheet
  PROJECT_PATH ${CMAKE_CURRENT_LIST_DIR})

add_shards_module(spreadsheet
  RUST_TARGETS shards-spreadsheet-rust
  REGISTER_SHARDS rust
  EXPERIMENTAL)
