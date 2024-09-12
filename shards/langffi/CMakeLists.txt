# Manually add lang source files as dependencies
set(LANG_PATH ../lang)
file(GLOB_RECURSE LANG_DEPS ${LANG_PATH}/*.rs ${LANG_PATH}/Cargo.toml)
file(GLOB_RECURSE LANG_DEPS_REMOVE ${LANG_PATH}/target/*.rs)
message(DEBUG ${LANG_DEPS_REMOVE})
list(REMOVE_ITEM LANG_DEPS ${LANG_DEPS_REMOVE})

add_rust_library(
  NAME shards-langffi
  PROJECT_PATH ${CMAKE_CURRENT_LIST_DIR}
  DEPENDS ${SHARDS_DIR}/include/shards/shards.h ${LANG_DEPS}
)

add_shards_module(langffi
  RUST_TARGETS shards-langffi-rust
  REGISTER_SHARDS langffi)