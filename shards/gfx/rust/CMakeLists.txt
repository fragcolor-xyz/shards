set(WGPU_NATIVE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/wgpu-native)
file(GLOB_RECURSE EXTRA_SOURCES
  ${WGPU_NATIVE_PATH}/ffi/*.h
  ${WGPU_NATIVE_PATH}/src/*.rs)

file(GLOB_RECURSE EXTRA_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/naga-native/*.rs
  ${CMAKE_CURRENT_SOURCE_DIR}/naga-native/Cargo.toml)

if(TRACY_ENABLE)
  message(STATUS "Building gfx crate with tracy profiling enabled")
  list(APPEND FEATURES tracy)
  list(APPEND RUST_ENV
  TRACY_CLIENT_LIB=$<TARGET_FILE_NAME:Tracy::TracyClient>
    TRACY_CLIENT_LIB_PATH=$<TARGET_FILE_DIR:Tracy::TracyClient>)
endif()

set(GFX_BINDINGS_FILE ${CMAKE_CURRENT_SOURCE_DIR}/gfx/bindings.hpp)
set_source_files_properties(${GFX_BINDINGS_FILE} PROPERTIES GENERATED TRUE)

add_rust_library(
  NAME gfx
  OUTPUTS ${GFX_BINDINGS_FILE}
  PROJECT_PATH ${CMAKE_CURRENT_SOURCE_DIR}/gfx
  DEPENDS ${EXTRA_SOURCES}
  FEATURES ${FEATURES}
  ENVIRONMENT ${RUST_ENV} CARGO_FEATURE_STD=YES
)

if(WIN32)
  target_link_libraries(gfx-rust INTERFACE Userenv ws2_32 Bcrypt d3d12 d3dcompiler)
endif()

if(HAS_WGPU_NATIVE)
  target_compile_definitions(gfx-rust INTERFACE WEBGPU_NATIVE=1)
  target_include_directories(gfx-rust INTERFACE ${WGPU_NATIVE_PATH}/ffi)
  target_include_directories(gfx-rust INTERFACE ${WGPU_NATIVE_PATH}/ffi/webgpu-headers)
endif()
