add_rust_library(
  NAME shards-langffi
  PROJECT_PATH ${CMAKE_CURRENT_LIST_DIR}
  DEPENDS ${SHARDS_DIR}/include/shards/shards.h
)

add_shards_module(langffi
  RUST_TARGETS shards-langffi-rust
  REGISTER_SHARDS langffi)