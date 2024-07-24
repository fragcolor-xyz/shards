set(SOURCES
  clipboard.cpp
)

add_shards_module(clipboard SOURCES ${SOURCES}
  REGISTER_SHARDS clipboard)

target_link_libraries(shards-module-clipboard gfx-core shards-module-gfx)
