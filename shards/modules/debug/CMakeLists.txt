set(SOURCES
  debug.cpp
)

add_shards_module(debug SOURCES ${SOURCES}
  REGISTER_SHARDS debug)
