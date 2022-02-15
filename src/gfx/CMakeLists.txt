set(BGFX_DIR ${CHAINBLOCKS_DIR}/deps/bgfx)

option(GFX_DEBUG "Enable context debug mode by default" OFF)

add_library(gfx
  context.cpp
  context_data.cpp
  window.cpp
  loop.cpp
  view.cpp
  gfx_wgpu.cpp
  renderer.cpp
  mesh.cpp
  geom.cpp
  drawable.cpp
  enums.cpp
  params.cpp
  paths.cpp
  utils.cpp
  feature.cpp
  texture.cpp
  shader/generator.cpp
  shader/blocks.cpp
  # cache_data.cpp
)

target_compile_features(gfx PUBLIC cxx_std_20)

target_include_directories(gfx
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(gfx
  tinygltf
  magic_enum
  spdlog stb xxHash Boost::headers
)
target_precompile_headers(gfx PUBLIC "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/pch.cpp>")

add_library(gfx_texture_file texture_file.cpp)
target_link_libraries(gfx_texture_file gfx)

add_library(gfx_imgui
  imgui.cpp
  imgui_impl_wgpu.cpp
  imgui_impl_sdl.cpp
)
target_link_libraries(gfx_imgui
  gfx
  imgui imgui_club implot imguizmo
)

option(GFX_USE_DATA_PATH "Uses build tree paths to find data (dev only)" ON)

if(NOT EMSCRIPTEN AND (CMAKE_BUILD_TYPE MATCHES Debug))
	set(GFX_CAN_USE_DATA_PATH ON)
endif()
if(GFX_CAN_USE_DATA_PATH AND GFX_USE_DATA_PATH)
  target_compile_definitions(gfx PUBLIC GFX_DATA_PATH=\"${CHAINBLOCKS_DIR}\")
endif()

if(GFX_DEBUG)
	target_compile_definitions(gfx PUBLIC GFX_DEBUG=1)
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
else()
  target_link_libraries(gfx SDL2-static)
endif()

if(NOT EMSCRIPTEN)
  # wgpu-native target
  if(Rust_CARGO_TARGET)
    set(TARGET_SWITCH --target ${Rust_CARGO_TARGET})
  endif()

  set(WGPU_NATIVE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/wgpu-native)
  set(GENERATED_LIB_PATH ${WGPU_NATIVE_PATH}/target/${Rust_BUILD_SUBDIR}/${Rust_LIB_PREFIX}wgpu_native${Rust_LIB_SUFFIX})
  file(GLOB_RECURSE SOURCES ${WGPU_NATIVE_PATH}/*.rs ${WGPU_NATIVE_PATH}/*.toml)

  add_custom_command(
    OUTPUT ${GENERATED_LIB_PATH}
    COMMAND ${CMAKE_COMMAND} -E env RUSTFLAGS="${Rust_FLAGS} -g" cargo ${Rust_CARGO_TOOLCHAIN} build ${Rust_CARGO_UNSTABLE_FLAGS} ${TARGET_SWITCH} ${Rust_CARGO_FLAGS}
    WORKING_DIRECTORY ${WGPU_NATIVE_PATH}
    DEPENDS ${SOURCES}
    USES_TERMINAL
  )
  add_custom_target(
    cargo-wgpu-native
    DEPENDS ${GENERATED_LIB_PATH}
  )

  add_library(wgpu-native STATIC IMPORTED GLOBAL)
  add_dependencies(wgpu-native cargo-wgpu-native)
  set_target_properties(wgpu-native PROPERTIES
    IMPORTED_LOCATION ${GENERATED_LIB_PATH}
  )
  target_include_directories(wgpu-native INTERFACE ${WGPU_NATIVE_PATH}/ffi)
  target_link_libraries(wgpu-native INTERFACE Userenv ws2_32 Bcrypt d3d12 d3dcompiler)
  # Userenv ws2_32 Bcrypt Crypt32 Secur32 NtDll Ncrypt

  target_link_libraries(gfx wgpu-native)
  target_compile_definitions(gfx PUBLIC WEBGPU_NATIVE=1)
endif()


add_subdirectory(tests)
