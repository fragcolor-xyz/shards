set(SOURCES
  reflection.cpp
)

add_shards_module(reflection SOURCES ${SOURCES}
  REGISTER_SHARDS reflection
  EXPERIMENTAL)
