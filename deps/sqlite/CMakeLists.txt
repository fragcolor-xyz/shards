set(SQLITE_SOURCES
  sqlite3.c
)

add_library(sqlite-static STATIC ${SQLITE_SOURCES})
target_compile_definitions(sqlite-static PUBLIC SQLITE_THREADSAFE=0)
target_include_directories(sqlite-static PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})