set(SOURCES
  sqlite.cpp
)

add_shards_module(sqlite SOURCES ${SOURCES}
  REGISTER_SHARDS sqlite)

# Manually specify the libraries in the correct order and repeat to resolve circular dependencies
# This is to avoid using LINK_GROUP which is not supported by older cmake version
target_link_libraries(shards-module-sqlite
  crsql_bundle-rust
  crsqlite-extension
  sqlite-static
)
