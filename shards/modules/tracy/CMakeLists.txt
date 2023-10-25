set(SOURCES
  tracy.cpp
)

add_shards_module(tracy SOURCES ${SOURCES}
  REGISTER_SHARDS tracy)

target_link_libraries(shards-module-tracy Tracy::TracyClient)
