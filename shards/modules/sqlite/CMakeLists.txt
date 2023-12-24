set(SOURCES
  sqlite.cpp
)

if(NOT EMSCRIPTEN)
  list(APPEND DB_RUST_FEATURES "cozo-full")
endif()

add_rust_library(NAME shards-sqlite
  PROJECT_PATH ${CMAKE_CURRENT_LIST_DIR}
  FEATURES ${DB_RUST_FEATURES})

add_shards_module(sqlite SOURCES ${SOURCES}
  REGISTER_SHARDS sqlite rust
  RUST_TARGETS shards-sqlite-rust)

target_link_libraries(shards-module-sqlite sqlite-static)
