set(physics_SOURCES
  physics.cpp
  shapes.cpp
  debug.cpp
  body.cpp
  constraints.cpp
  soft_body.cpp
)

add_shards_module(physics
  SOURCES ${physics_SOURCES}
  REGISTER_SHARDS physics shapes debug body soft_body constraints)

target_link_libraries(shards-module-physics
  Jolt
  gfx-headers
)

target_include_directories(shards-module-physics
  PUBLIC ${JoltPhysics_SOURCE_DIR}/TestFramework
  PUBLIC ${JoltPhysics_SOURCE_DIR}/Samples
  PRIVATE ${JoltPhysics_SOURCE_DIR}/Samples
)
