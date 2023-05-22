set(SOURCES
  edn.cpp
)

add_shards_module(edn SOURCES ${SOURCES}
  REGISTER_SHARDS edn
  EXPERIMENTAL)
