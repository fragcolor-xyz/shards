set(EXTRA_SOURCES ../../include/shards/shards.h)

add_rust_library(
  NAME shards
  PROJECT_PATH ${CMAKE_CURRENT_LIST_DIR}
  DEPENDS ${EXTRA_SOURCES}
)
