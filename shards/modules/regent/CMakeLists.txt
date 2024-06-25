set(regent_SOURCES
  regent_shards.cpp
  regent.cpp
)

add_shards_module(regent
  SOURCES ${regent_SOURCES}
  REGISTER_SHARDS regent)

target_link_libraries(shards-module-regent
  linalg shards-logging
  TracyClient
)
