add_rust_library(
  NAME shards-lang
  PROJECT_PATH ${CMAKE_CURRENT_LIST_DIR}
)

add_shards_module(lang
  RUST_TARGETS shards-lang-rust
  REGISTER_SHARDS lang)
