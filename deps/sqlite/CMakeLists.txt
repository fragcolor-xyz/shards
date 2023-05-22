set(SQLITE_SOURCES
  cr-sqlite/core/src/sqlite/sqlite3.c
  cr-sqlite/core/src/util.c
	cr-sqlite/core/src/tableinfo.c
	cr-sqlite/core/src/triggers.c
  cr-sqlite/core/src/crsqlite.c
	cr-sqlite/core/src/changes-vtab.c
	cr-sqlite/core/src/changes-vtab-read.c
	cr-sqlite/core/src/changes-vtab-common.c
	cr-sqlite/core/src/changes-vtab-write.c
	cr-sqlite/core/src/ext-data.c
	cr-sqlite/core/src/get-table.c
)

add_library(sqlite-static STATIC ${SQLITE_SOURCES})
target_compile_definitions(sqlite-static PUBLIC SQLITE_THREADSAFE=2 SQLITE_ENABLE_SESSION=1)
target_include_directories(sqlite-static PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/cr-sqlite/core/src/sqlite)
target_include_directories(sqlite-static PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/cr-sqlite/core/src)

add_rust_library(NAME crsql_bundle
  FEATURES static
  PROJECT_PATH ${SHARDS_DIR}/deps/sqlite/cr-sqlite/core/rs/bundle)
target_link_libraries(sqlite-static crsql_bundle-rust)
