set(extra_SOURCES
  runtime.cpp
  # bgfx.cpp
  # imgui.cpp
  # xr.cpp
  # gizmo.cpp
  dsp.cpp
  # inputs.cpp
  snappy.cpp
  brotli.cpp
)

if(CHAINBLOCKS_EXTRA_BUILD_SHARED)
  add_library(chainblocks-extra SHARED ${extra_SOURCES})
  target_compile_definitions(chainblocks-extra PUBLIC "-DCHAINBLOCKS_EXTRA_DLL=1")
else()
  add_library(chainblocks-extra STATIC ${extra_SOURCES})
endif()

if(EMSCRIPTEN)
  # Use builtin SDL2 port
  target_compile_options(chainblocks-extra PUBLIC
    "SHELL:-s USE_SDL=2"
  )
  target_link_options(chainblocks-extra PUBLIC
    "SHELL:-s MIN_WEBGL_VERSION=2"
    "SHELL:-s MAX_WEBGL_VERSION=2"
    "SHELL:-s USE_SDL=2"
  )
else()
  target_link_libraries(chainblocks-extra SDL2-static)
endif()

target_link_libraries(chainblocks-extra
  chainblocks-core
  stb
  brotlienc-static brotlidec-static brotlicommon-static snappy
  kissfft miniaudio
  nlohmann_json
)

if(WIN32)
  target_link_libraries(chainblocks-extra DXGI D3D11 ntdll)
  target_sources(chainblocks-extra PRIVATE desktop.win.cpp)
endif()

# Need to compile specific files as objective-c
if(IOS)
  target_sources(chainblocks-extra PRIVATE audio.mm)
else()
  target_sources(chainblocks-extra PRIVATE audio.cpp)
endif()

option(CHAINBLOCKS_EXTRA_BUILD_SHARED OFF "Build chainblocks extra as dynamic library")

if(CHAINBLOCKS_WITH_RUST_BLOCKS)
  target_compile_definitions(chainblocks-extra PUBLIC "-DCHAINBLOCKS_WITH_RUST_BLOCKS=1")
  target_link_libraries(chainblocks-extra chainblocks-rust)
endif()


if(CHAINBLOCKS_BUILD_TESTS)
  # Include test code embedded in chainblocks-extra
  target_compile_definitions(chainblocks-extra PRIVATE CHAINBLOCKS_BUILD_TESTS=1)
  target_link_libraries(chainblocks-extra Catch2)

  add_executable(test_extra
    ../tests/test_extra.cpp
  )

  target_link_libraries(test_extra
    chainblocks-core chainblocks-extra Catch2 Catch2WithMain
  )
endif()
