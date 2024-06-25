set(physics_SOURCES
  physics.cpp
  shapes.cpp
  debug.cpp
)

add_shards_module(physics
  SOURCES ${physics_SOURCES}
  REGISTER_SHARDS physics shapes debug)

target_link_libraries(shards-module-physics
  Jolt
  gfx # For glTF interface
)
