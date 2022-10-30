set(WGPU_NATIVE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/wgpu-native)
file(GLOB_RECURSE EXTRA_SOURCES
  ${WGPU_NATIVE_PATH}/ffi/*.h
  ${WGPU_NATIVE_PATH}/src/*.rs
  ${CMAKE_CURRENT_SOURCE_DIR}/gfx/ffi/*.h)

if(NOT EMSCRIPTEN)
  list(APPEND FEATURES wgpu)
  set(HAS_WGPU_NATIVE ON)

  # For now expose the extra vulkan API everywhere for OpenXR
  list(APPEND FEATURES vulkan)
endif()

add_rust_library(
  NAME gfx
  PROJECT_PATH ${CMAKE_CURRENT_SOURCE_DIR}/gfx
  DEPENDS ${EXTRA_SOURCES}
  FEATURES ${FEATURES}
)

if(WIN32)
  target_link_libraries(gfx-rust INTERFACE Userenv ws2_32 Bcrypt d3d12 d3dcompiler)
endif()

if(HAS_WGPU_NATIVE)
  target_compile_definitions(gfx-rust INTERFACE WEBGPU_NATIVE=1)
  target_include_directories(gfx-rust INTERFACE ${WGPU_NATIVE_PATH}/ffi)
  target_include_directories(gfx-rust INTERFACE ${CMAKE_CURRENT_LIST_DIR}/gfx/ffi)
endif()
