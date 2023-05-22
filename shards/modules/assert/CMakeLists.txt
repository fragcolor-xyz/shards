set(SOURCES
  assert.cpp
)

add_shards_module(assert SOURCES ${SOURCES}
  REGISTER_SHARDS assert)

# target_include_directories(shards-module-assert PRIVATE ../core)
