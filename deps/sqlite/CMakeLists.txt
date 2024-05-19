set(SQLITE_SOURCES
  cr-sqlite/core/src/sqlite/sqlite3.c
)

set(SQLITE_DEFINES
  SQLITE_CORE=1 # This makes the extension static
  SQLITE_THREADSAFE=2 SQLITE_DEFAULT_MEMSTATUS=0 SQLITE_DEFAULT_WAL_SYNCHRONOUS=1
  SQLITE_MAX_EXPR_DEPTH=0 SQLITE_OMIT_DECLTYPE SQLITE_OMIT_DEPRECATED SQLITE_OMIT_DESERIALIZE SQLITE_OMIT_PROGRESS_CALLBACK
  SQLITE_ENABLE_FTS5=1 SQLITE_ENABLE_RTREE=1 SQLITE_ENABLE_GEOPOLY=1
  SQLITE_LIKE_DOESNT_MATCH_BLOBS SQLITE_OMIT_SHARED_CACHE)

add_library(sqlite-static STATIC ${SQLITE_SOURCES})
target_compile_definitions(sqlite-static PUBLIC ${SQLITE_DEFINES})
target_include_directories(sqlite-static PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/cr-sqlite/core/src/sqlite)

add_rust_library(NAME crsql_bundle
  PROJECT_PATH ${CMAKE_CURRENT_LIST_DIR}/cr-sqlite/core/rs/bundle
  ENVIRONMENT ${RUST_ENV} CRSQLITE_COMMIT_SHA=shards-dev
  FEATURES static)

add_library(crsqlite-extension STATIC
  cr-sqlite/core/src/sqlite/sqlite3.c
  cr-sqlite/core/src/crsqlite.c
  cr-sqlite/core/src/changes-vtab.c
  cr-sqlite/core/src/ext-data.c
)
target_link_libraries(crsqlite-extension PUBLIC crsql_bundle-rust)
target_compile_definitions(crsqlite-extension PUBLIC ${SQLITE_DEFINES})
target_include_directories(crsqlite-extension PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/cr-sqlite/core/src/sqlite)
# target_link_libraries(crsql_bundle-rust crsqlite-extension)