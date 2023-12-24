set(SOURCES
  sqlite.cpp
)

add_rust_library(NAME shards-db
  PROJECT_PATH ${CMAKE_CURRENT_LIST_DIR}
  FEATURES ${FS_RUST_FEATURES})

add_shards_module(sqlite SOURCES ${SOURCES}
  REGISTER_SHARDS sqlite
  RUST_TARGETS shards-db-rust)

target_link_libraries(shards-module-sqlite sqlite-static)
