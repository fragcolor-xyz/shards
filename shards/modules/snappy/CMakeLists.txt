set(SOURCES
  snappy.cpp
)

add_shards_module(snappy SOURCES ${SOURCES}
  REGISTER_SHARDS snappy)

target_link_libraries(shards-module-snappy snappy)
