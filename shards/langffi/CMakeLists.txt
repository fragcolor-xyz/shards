# if debug build , pure debug use feature large-min-stack, else no features
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  add_rust_library(
    NAME shards-langffi
    PROJECT_PATH ${CMAKE_CURRENT_LIST_DIR}
    DEPENDS ${SHARDS_DIR}/include/shards/shards.h
    FEATURES large-min-stack
  )
else()
  add_rust_library(
    NAME shards-langffi
    PROJECT_PATH ${CMAKE_CURRENT_LIST_DIR}
    DEPENDS ${SHARDS_DIR}/include/shards/shards.h
  )
endif()

add_shards_module(langffi
  RUST_TARGETS shards-langffi-rust
  REGISTER_SHARDS langffi)