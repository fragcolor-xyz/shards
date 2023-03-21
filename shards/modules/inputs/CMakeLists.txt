set(SOURCES
  inputs.cpp
  inputs1.cpp
)

add_shards_module(inputs SOURCES ${SOURCES}
  REGISTER_SHARDS inputs inputs1)

target_link_libraries(shards-module-inputs gfx input)
