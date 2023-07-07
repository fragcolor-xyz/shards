add_rust_library(NAME shards-crypto 
  PROJECT_PATH ${CMAKE_CURRENT_LIST_DIR})

add_shards_module(crypto 
  RUST_TARGETS shards-crypto-rust
  REGISTER_SHARDS crypto)
