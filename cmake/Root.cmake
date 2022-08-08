get_filename_component(SHARDS_DIR ${CMAKE_CURRENT_LIST_DIR}/.. ABSOLUTE)
message(STATUS "SHARDS_DIR = ${SHARDS_DIR}")

option(SHARDS_BUILD_TESTS "Enable to build shards tests" ON)
option(SHARDS_WITH_EXTRA_SHARDS "Enabled to build and include the extra c++ shards (graphics, audio, etc.)" ON)
option(SHARDS_WITH_RUST_SHARDS "Enable to build and include the extra rust shards" ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

list(APPEND CMAKE_MODULE_PATH "${SHARDS_DIR}/cmake")
list(APPEND CMAKE_MODULE_PATH "${SHARDS_DIR}/cmake/android-cmake")

include(Sanitizers)
include(Platform)
include(Tidy)
include(Utils)
include(Tools)

# Rust build support
include(Rust)

# disable targets being added by CTestTargets.cmake
set_property(GLOBAL PROPERTY CTEST_TARGETS_ADDED 1)

add_subdirectory(${SHARDS_DIR}/deps deps)

add_subdirectory(${SHARDS_DIR}/src/core src/core)
add_subdirectory(${SHARDS_DIR}/src/mal src/mal)
add_subdirectory(${SHARDS_DIR}/src/gfx src/gfx)

if(SHARDS_WITH_EXTRA_SHARDS)
  if(SHARDS_WITH_RUST_SHARDS)
    add_subdirectory(${SHARDS_DIR}/rust rust)
  endif()

  add_subdirectory(${SHARDS_DIR}/src/extra src/extra)
endif()
