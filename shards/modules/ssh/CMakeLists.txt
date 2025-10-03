add_rust_library(NAME shards-ssh
  PROJECT_PATH ${CMAKE_CURRENT_LIST_DIR})

add_shards_module(ssh
  RUST_TARGETS shards-ssh-rust
  REGISTER_SHARDS rust)
