set(SOURCES
  random.cpp
)

add_rust_library(NAME shards-random
  PROJECT_PATH ${CMAKE_CURRENT_LIST_DIR})

add_shards_module(random SOURCES ${SOURCES}
  REGISTER_SHARDS random rust
  RUST_TARGETS shards-random-rust)
