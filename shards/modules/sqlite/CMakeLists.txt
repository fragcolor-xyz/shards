set(SOURCES
  sqlite.cpp
)

add_shards_module(sqlite SOURCES ${SOURCES}
  REGISTER_SHARDS sqlite
)
target_link_libraries(shards-module-sqlite)

# Link CR-SQLite extension (pure C)
target_link_libraries(shards-module-sqlite crsqlite-extension)
