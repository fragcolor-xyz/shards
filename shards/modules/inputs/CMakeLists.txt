set(SOURCES
  inputs.cpp
  detached.cpp
)

add_shards_module(inputs SOURCES ${SOURCES}
  REGISTER_SHARDS inputs inputs_detached ${INPUT_RUST_REGISTER}
  RUST_TARGETS ${INPUT_RUST_TARGETS})

target_link_libraries(shards-module-inputs gfx-core input) 
