add_rust_library(NAME shards-crdts
  PROJECT_PATH ${CMAKE_CURRENT_LIST_DIR})

add_shards_module(crdts
  RUST_TARGETS shards-crdts-rust
  REGISTER_SHARDS crdts)

target_include_directories(shards-module-crdts
  PUBLIC ${SHARDS_DIR}/deps/crdt-lite
)