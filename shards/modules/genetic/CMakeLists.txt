set(SOURCES
  genetic.cpp
)

add_shards_module(genetic SOURCES ${SOURCES}
  REGISTER_SHARDS genetic
  EXPERIMENTAL)
