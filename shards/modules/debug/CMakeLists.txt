set(SOURCES
  debug.cpp
  wires.cpp
)

add_shards_module(debug SOURCES ${SOURCES}
  REGISTER_SHARDS debug wires)
