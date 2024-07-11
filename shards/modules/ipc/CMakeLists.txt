add_rust_library(NAME shards-ipc
  PROJECT_PATH ${CMAKE_CURRENT_LIST_DIR})

add_shards_module(ipc
  SOURCES schemes.cpp
  REGISTER_SHARDS rust schemes
  RUST_TARGETS shards-ipc-rust
  EXPERIMENTAL)
