set(SOURCES
  channels.cpp
  events.cpp
)

add_shards_module(channels SOURCES ${SOURCES}
  REGISTER_SHARDS channels events)

# target_include_directories(shards-module-channels PRIVATE ../core)
