# Define SQLite source files
set(SQLITE_SOURCES
  cr-sqlite/core/src/sqlite/sqlite3.c
)

# Define SQLite compilation definitions
set(SQLITE_DEFINES
  SQLITE_CORE=1
  SQLITE_THREADSAFE=2
  SQLITE_DEFAULT_MEMSTATUS=0
  SQLITE_DEFAULT_WAL_SYNCHRONOUS=1
  SQLITE_MAX_EXPR_DEPTH=0
  SQLITE_OMIT_DECLTYPE
  SQLITE_OMIT_DEPRECATED
  SQLITE_OMIT_DESERIALIZE
  SQLITE_OMIT_PROGRESS_CALLBACK
  SQLITE_LIKE_DOESNT_MATCH_BLOBS
  SQLITE_OMIT_SHARED_CACHE
  # Uncomment to enable additional features:
  SQLITE_ENABLE_FTS5=1
  SQLITE_ENABLE_RTREE=1
  SQLITE_ENABLE_GEOPOLY=1
)

# Add Emscripten-specific definitions
if(EMSCRIPTEN)
  list(APPEND SQLITE_DEFINES SQLITE_ENABLE_FLOCK=1)
endif()

# Create static SQLite library
add_library(sqlite-static STATIC ${SQLITE_SOURCES})
target_compile_definitions(sqlite-static PUBLIC ${SQLITE_DEFINES})
target_include_directories(sqlite-static PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/cr-sqlite/core/src/sqlite)

# Add Rust library for CR-SQLite bundle
add_rust_library(
  NAME crsql_bundle
  PROJECT_PATH ${CMAKE_CURRENT_LIST_DIR}/cr-sqlite/core/rs/bundle
  ENVIRONMENT ${RUST_ENV} CRSQLITE_COMMIT_SHA=shards-dev
  FEATURES static
)

# Option to include SQLite vector extension
option(WITH_SQLITE_VEC "Include SQLite vector extension" ON)

if(WITH_SQLITE_VEC)
  # Create SQLite vector extension library
  add_library(sqlite-vec STATIC ${CMAKE_CURRENT_SOURCE_DIR}/sqlite-vec/sqlite-vec.c)
  target_compile_definitions(sqlite-vec PRIVATE ${SQLITE_DEFINES})
  target_include_directories(sqlite-vec PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/sqlite-vec-static)
  target_include_directories(sqlite-vec PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/cr-sqlite/core/src/sqlite)

  # Add architecture-specific compile definitions for SQLite vector extension
  if(ARM_NEON)
    target_compile_definitions(sqlite-vec PRIVATE SQLITE_VEC_ENABLE_NEON)
  elseif(X86 AND CPUBITS64)
    target_compile_definitions(sqlite-vec PRIVATE SQLITE_VEC_ENABLE_AVX)
  endif()
endif()

# Create CR-SQLite extension object library
add_library(crsqlite-extension-obj OBJECT
  cr-sqlite/core/src/crsqlite.c
  cr-sqlite/core/src/changes-vtab.c
  cr-sqlite/core/src/ext-data.c
)
target_link_libraries(crsqlite-extension-obj PUBLIC sqlite-static)
if(WITH_SQLITE_VEC)
  target_link_libraries(crsqlite-extension-obj PUBLIC sqlite-vec)
  target_compile_definitions(crsqlite-extension-obj PUBLIC WITH_SQLITE_VEC)
endif()
target_compile_definitions(crsqlite-extension-obj PUBLIC ${SQLITE_DEFINES})
target_include_directories(crsqlite-extension-obj PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/cr-sqlite/core/src/sqlite)

# Create interface library for CR-SQLite extension
add_library(crsqlite-extension INTERFACE)
target_sources(crsqlite-extension INTERFACE $<TARGET_OBJECTS:crsqlite-extension-obj>)
target_link_libraries(crsqlite-extension INTERFACE crsqlite-extension-obj)
