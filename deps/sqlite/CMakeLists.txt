set(SQLITE_SOURCES
  cr-sqlite/core/src/sqlite/sqlite3.c
)

add_library(sqlite-static STATIC ${SQLITE_SOURCES})
target_compile_definitions(sqlite-static PUBLIC SQLITE_THREADSAFE=2 SQLITE_ENABLE_SESSION=1 SQLITE_OMIT_DECLTYPE SQLITE_OMIT_DEPRECATED SQLITE_OMIT_DESERIALIZE SQLITE_OMIT_PROGRESS_CALLBACK)
target_include_directories(sqlite-static PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/cr-sqlite/core/src/sqlite)
