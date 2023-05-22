add_shards_module(crypto 
  REGISTER_SHARDS crypto)

add_rust_library(NAME shards-crypto 
  PROJECT_PATH ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(shards-module-crypto shards-crypto-rust)
