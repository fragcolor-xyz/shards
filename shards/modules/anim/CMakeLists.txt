set(SOURCES
  anim.cpp
)

add_shards_module(anim SOURCES ${SOURCES}
  REGISTER_SHARDS anim)
target_include_directories(shards-module-anim PRIVATE 
  ${SHARDS_DIR}/anim
  ${SHARDS_DIR}/shards # For gfx
  ${SHARDS_DIR}/gfx # For gfx
)
