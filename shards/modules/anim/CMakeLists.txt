set(SOURCES
  anim.cpp
)

add_shards_module(anim SOURCES ${SOURCES}
  REGISTER_SHARDS anim)
target_link_libraries(shards-module-anim gfx-core)
