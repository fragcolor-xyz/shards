add_rust_library(NAME shards-csv
  PROJECT_PATH ${CMAKE_CURRENT_LIST_DIR})

add_shards_module(csv
  RUST_TARGETS shards-csv-rust
  REGISTER_SHARDS rust
  EXPERIMENTAL)
