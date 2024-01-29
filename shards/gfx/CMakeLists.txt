option(SH_DEBUG "Enable context debug mode by default" OFF)

add_library(gfx
  culling.cpp
  context.cpp
  context_data.cpp
  window.cpp
  loop.cpp
  view.cpp
  gfx_wgpu.cpp
  renderer.cpp
  mesh.cpp
  mesh_utils.cpp
  geom.cpp
  drawable.cpp
  enums.cpp
  params.cpp
  paths.cpp
  utils.cpp
  feature.cpp
  texture.cpp
  material.cpp
  drawables/mesh_drawable.cpp
  drawables/mesh_tree_drawable.cpp
  gizmos/wireframe.cpp
  gizmos/shapes.cpp
  gizmos/gizmo_input.cpp
  gizmos/gizmos.cpp
  shader/generator.cpp
  shader/generator_context.cpp
  shader/blocks.cpp
  shader/types.cpp
  features/velocity.cpp
  features/transform.cpp
  rust_interop.cpp
  view_stack.cpp
  render_target.cpp
  render_graph_evaluator.cpp
  render_step_impl.cpp
  renderer_types.cpp
  pipeline_builder.cpp
  pipeline_step.cpp
  pipeline_hashes.cpp
  unique_id.cpp
)

# Always optimize files with hot-paths
set_source_files_properties(
  drawables/mesh_tree_drawable.cpp
  gltf/gltf.cpp
  texture_file/texture_file.cpp
  pipeline_hashes.cpp
  PROPERTIES
    COMPILE_FLAGS -O3
    SKIP_PRECOMPILE_HEADERS ON)

target_compile_features(gfx PUBLIC cxx_std_20)

target_include_directories(gfx
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../..
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(gfx
  tinygltf draco-wrapper
  magic_enum nameof linalg
  spdlog stb xxHash Boost::algorithm Boost::filesystem Boost::container Boost::tti
  shards-logging
  shards-fast-string
)
target_precompile_headers(gfx PUBLIC "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/pch.cpp>")
target_link_libraries(gfx Tracy::TracyClient)

if(HAVE_THREADS)
  set(GFX_THREADING_DEFAULT ON)
endif()
option(GFX_THREADING "Support multithreaded rendering" ${GFX_THREADING_DEFAULT})
if(GFX_THREADING)
  target_compile_definitions(gfx PUBLIC GFX_THREADING_SUPPORT=1)
endif()

target_link_libraries(gfx Taskflow)

add_library(gfx-texture-file texture_file/texture_file.cpp)
target_link_libraries(gfx-texture-file gfx)

add_library(gfx-gltf gltf/gltf.cpp)
target_link_libraries(gfx-gltf gfx tinygltf)
target_link_libraries(gfx-gltf Tracy::TracyClient)

option(GFX_USE_DATA_PATH "Uses build tree paths to find data (dev only)" ON)

if(NOT EMSCRIPTEN AND (CMAKE_BUILD_TYPE MATCHES Debug))
	set(GFX_CAN_USE_DATA_PATH ON)
endif()
if(GFX_CAN_USE_DATA_PATH AND GFX_USE_DATA_PATH)
  target_compile_definitions(gfx PUBLIC GFX_DATA_PATH=\"${SHARDS_DIR}\")
endif()

if(EMSCRIPTEN)
  # Use builtin SDL2 port
  target_compile_options(gfx PUBLIC
    "SHELL:-s USE_SDL=2"
  )

  target_link_options(gfx PUBLIC
    "SHELL:-s MIN_WEBGL_VERSION=2"
    "SHELL:-s MAX_WEBGL_VERSION=2"
    "SHELL:-s USE_SDL=2"
    "SHELL:-s USE_WEBGPU=1"
    "SHELL:-s ALLOW_MEMORY_GROWTH=1"
    "SHELL:-s NO_DISABLE_EXCEPTION_CATCHING"
    "SHELL:-s ASYNCIFY=1"
  )

  if(CMAKE_BUILD_TYPE MATCHES "Debug")
    target_link_options(gfx PUBLIC
      "SHELL:-s EXCEPTION_DEBUG=1"
    )
  endif()

  if(EMSCRIPTEN)
    em_link_js_library(gfx gfx_wgpu.js)
  endif()
else()
  target_link_libraries(gfx SDL2)
  if(NOT DESKTOP)
    # Only need SDL2main for setup on mobile platforms
    target_link_libraries(gfx SDL2main)
  endif()

  set(HAS_WGPU_NATIVE ON)
endif()

if(ANDROID)
  target_link_libraries(gfx android OpenSLES)
endif()

if(HAS_WGPU_NATIVE)
  add_subdirectory(rust)
  target_link_libraries(gfx gfx-rust)
  target_sources(gfx PRIVATE ${GFX_BINDINGS_FILE})
endif()

add_subdirectory(tests)
