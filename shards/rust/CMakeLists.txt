set(EXTRA_SOURCES ../../include/shards/shards.h)

if((CMAKE_BUILD_TYPE STREQUAL "Release") OR (CMAKE_BUILD_TYPE STREQUAL "MinSizeRel"))
  set(DEBUG_FEATURE "")
else()
  set(DEBUG_FEATURE "release_with_debug")
endif()

add_rust_library(
  NAME shards
  PROJECT_PATH ${CMAKE_CURRENT_LIST_DIR}
  DEPENDS ${EXTRA_SOURCES}
  FEATURES ${DEBUG_FEATURE}
)
