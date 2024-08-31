if(APPLE)
  list(APPEND ML_RUST_FEATURES "metal")
endif()

add_rust_library(NAME shards-ml
  PROJECT_PATH ${CMAKE_CURRENT_LIST_DIR}
  FEATURES ${ML_RUST_FEATURES})

add_shards_module(ml
  REGISTER_SHARDS rust
  RUST_TARGETS shards-ml-rust
  EXPERIMENTAL)
