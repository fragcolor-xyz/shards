set(SOURCES
  gfx.cpp
  window.cpp
  renderer.cpp
  mesh.cpp
  drawable.cpp
  material.cpp
  feature.cpp
  shader/translator.cpp
  shader/wgsl.cpp
  shader/composition.cpp
  shader/linalg_shards.cpp
  shader/math_shards.cpp
  shader/core_shards.cpp
  shader/wire_shards.cpp
  shader/flow_shards.cpp
  gltf.cpp
  camera.cpp
  texture.cpp
  view.cpp
  steps.cpp
  buffer.cpp
  gizmos/context.cpp
  gizmos/gizmos.cpp
  gizmos/highlight.cpp
  gizmos/shapes.cpp
)

add_shards_module(gfx SOURCES ${SOURCES}
  REGISTER_SHARDS gfx gizmos buffer)

target_link_libraries(shards-module-gfx
  stb gfx gfx-gltf)

target_precompile_headers(shards-module-gfx PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/pch.cpp>")
