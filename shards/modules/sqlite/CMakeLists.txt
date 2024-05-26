set(SOURCES
  sqlite.cpp
)

add_shards_module(sqlite SOURCES ${SOURCES}
  REGISTER_SHARDS sqlite)

# Circular dependency linking
if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
  target_link_libraries(shards-module-sqlite "$<LINK_GROUP:RESCAN,crsqlite-extension,sqlite-static,crsql_bundle-rust>")
else()
  target_link_libraries(shards-module-sqlite crsqlite-extension sqlite-static)
endif()
