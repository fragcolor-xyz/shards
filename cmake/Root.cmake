get_filename_component(CHAINBLOCKS_DIR ${CMAKE_CURRENT_LIST_DIR}/.. ABSOLUTE)
message(STATUS "CHAINBLOCKS_DIR = ${CHAINBLOCKS_DIR}")

option(CHAINBLOCKS_BUILD_TESTS "Enable to build chainblocks tests" ON)
option(CHAINBLOCKS_WITH_EXTRA_BLOCKS "Enabled to build and include the extra c++ blocks (graphics, audio, etc.)" ON)
option(CHAINBLOCKS_WITH_RUST_BLOCKS "Enable to build and include the extra rust blocks" ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CHAINBLOCKS_DIR}/cmake")

include(Sanitizers)
include(Platform)
include(Tidy)
include(Utils)

# Rust build support
include(Rust)

# disable targets being added by CTestTargets.cmake
set_property(GLOBAL PROPERTY CTEST_TARGETS_ADDED 1)

add_subdirectory(${CHAINBLOCKS_DIR}/deps deps)

add_subdirectory(${CHAINBLOCKS_DIR}/src/core src/core)
add_subdirectory(${CHAINBLOCKS_DIR}/src/mal src/mal)
add_subdirectory(${CHAINBLOCKS_DIR}/src/gfx src/gfx)

if(CHAINBLOCKS_WITH_EXTRA_BLOCKS)
  if(CHAINBLOCKS_WITH_RUST_BLOCKS)
    add_subdirectory(${CHAINBLOCKS_DIR}/rust rust)
  endif()

  add_subdirectory(${CHAINBLOCKS_DIR}/src/extra src/extra)
endif()
