set(SOURCES
  sqlite.cpp
)

add_shards_module(sqlite SOURCES ${SOURCES}
  REGISTER_SHARDS sqlite
  RUST_TARGETS crsql_bundle-rust
)

target_link_libraries(shards-module-sqlite crsqlite-extension-core sqlite-static)
