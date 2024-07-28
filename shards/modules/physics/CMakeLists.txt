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
  gfx # For glTF interface
)

target_include_directories(shards-module-physics
  PUBLIC ${SHARDS_DIR}/deps/JoltPhysics/TestFramework
  PUBLIC ${SHARDS_DIR}/deps/JoltPhysics/Samples
)
