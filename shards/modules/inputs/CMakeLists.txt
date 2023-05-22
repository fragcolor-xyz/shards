set(SOURCES
  inputs.cpp
)

add_shards_module(inputs SOURCES ${SOURCES}
  REGISTER_SHARDS inputs)

target_link_libraries(shards-module-inputs gfx input)
