set(extra_SOURCES
  runtime.cpp
  gfx.cpp
  gfx/main_window.cpp
  gfx/mesh.cpp
  gfx/drawable.cpp
  gfx/material.cpp
  gfx/feature.cpp
  gfx/shader/translator.cpp
  gfx/shader/wgsl.cpp
  gfx/shader/composition.cpp
  gfx/shader/linalg_shards.cpp
  gfx/shader/math_shards.cpp
  gfx/shader/core_shards.cpp
  gfx/shader/wire_shards.cpp
  gfx/shader/flow_shards.cpp
  gfx/gltf.cpp
  gfx/camera.cpp
  gfx/texture.cpp
  gfx/view.cpp
  gfx/steps.cpp
  spatial/spatial.cpp
  egui/pass.cpp
  rust_interop.cpp
  gizmos/context.cpp
  gizmos/gizmos.cpp
  gizmos/highlight.cpp
  gizmos/shapes.cpp
  # xr.cpp
  clipboard.cpp
  dsp.cpp
  inputs.cpp
  snappy.cpp
  brotli.cpp
  ../anim/types.cpp
  ../anim/anim.cpp
)

if(SHARDS_EXTRA_BUILD_SHARED)
  add_library(shards-extra SHARED ${extra_SOURCES})
  target_compile_definitions(shards-extra PUBLIC "-DSHARDS_EXTRA_DLL=1")
else()
  add_library(shards-extra STATIC ${extra_SOURCES})
endif()

target_link_libraries(shards-extra SDL2)

target_link_libraries(shards-extra
  shards-core
  stb gfx gfx-gltf gfx-egui input spatial
  brotlienc-static brotlidec-static brotlicommon-static snappy
  kissfft miniaudio
  nlohmann_json
)

if(WIN32)
  target_link_libraries(shards-extra DXGI D3D11 ntdll)
  target_sources(shards-extra PRIVATE desktop.win.cpp)
endif()

# Need to compile specific files as objective-c
if(IOS)
  target_sources(shards-extra PRIVATE audio.mm)
else()
  target_sources(shards-extra PRIVATE audio.cpp)
endif()

target_precompile_headers(shards-extra PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/pch.cpp>")

option(SHARDS_EXTRA_BUILD_SHARED OFF "Build shards extra as dynamic library")

if(SHARDS_WITH_RUST_SHARDS)
  target_compile_definitions(shards-extra PUBLIC "-DSHARDS_WITH_RUST_SHARDS=1")
  target_link_libraries(shards-extra shards-rust)

  # Because we have some references to c++ code from rust
  target_link_libraries(shards-rust INTERFACE shards-extra)
endif()


if(SHARDS_BUILD_TESTS)
  # Include test code embedded in shards-extra
  target_compile_definitions(shards-extra PRIVATE SHARDS_BUILD_TESTS=1)
  target_link_libraries(shards-extra Catch2)

  add_executable(test-extra
    ../tests/test_extra.cpp
  )

  target_link_libraries(test-extra
    shards-core shards-extra Catch2 Catch2Main
  )
endif()
