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
  gfx/gltf.cpp
  gui.cpp
  rust_interop.cpp
  imgui.cpp
  # xr.cpp
  # gizmo.cpp
  dsp.cpp
  inputs.cpp
  snappy.cpp
  brotli.cpp
)

if(SHARDS_EXTRA_BUILD_SHARED)
  add_library(shards-extra SHARED ${extra_SOURCES})
  target_compile_definitions(shards-extra PUBLIC "-DSHARDS_EXTRA_DLL=1")
else()
  add_library(shards-extra STATIC ${extra_SOURCES})
endif()

if(EMSCRIPTEN)
  # Use builtin SDL2 port
  target_compile_options(shards-extra PUBLIC
    "SHELL:-s USE_SDL=2"
  )
  target_link_options(shards-extra PUBLIC
    "SHELL:-s MIN_WEBGL_VERSION=2"
    "SHELL:-s MAX_WEBGL_VERSION=2"
    "SHELL:-s USE_SDL=2"
  )
else()
  target_link_libraries(shards-extra SDL2-static)
endif()

target_link_libraries(shards-extra
  shards-core
  stb gfx gfx_imgui gfx_gltf gfx_egui_interop
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

option(SHARDS_EXTRA_BUILD_SHARED OFF "Build shards extra as dynamic library")

if(SHARDS_WITH_RUST_SHARDS)
  target_compile_definitions(shards-extra PUBLIC "-DSHARDS_WITH_RUST_SHARDS=1")
  target_link_libraries(shards-extra shards-rust)
endif()


if(SHARDS_BUILD_TESTS)
  # Include test code embedded in shards-extra
  target_compile_definitions(shards-extra PRIVATE SHARDS_BUILD_TESTS=1)
  target_link_libraries(shards-extra Catch2)

  add_executable(test_extra
    ../tests/test_extra.cpp
  )

  target_link_libraries(test_extra
    shards-core shards-extra Catch2 Catch2Main
  )
endif()
