set(SOURCES
  random.cpp
)

add_shards_module(random SOURCES ${SOURCES}
  REGISTER_SHARDS random)
