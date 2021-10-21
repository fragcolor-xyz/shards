cmake_minimum_required(VERSION 3.14)

project("chainblocks")

option(CHAINBLOCKS_BUILD_TESTS "Enable to build chainblocks tests" ON)
option(CHAINBLOCKS_WITH_EXTRA_BLOCKS "Enabled to build and include the extra c++ blocks (graphics, audio, etc.)" ON)
option(CHAINBLOCKS_WITH_RUST_BLOCKS "Enable to build and include the extra rust blocks" ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

if (CMAKE_SYSTEM_PROCESSOR MATCHES "(x86)|(X86)|(amd64)|(AMD64)")
  set (X86 TRUE)
else()
  set (X86 FALSE) 
endif()

if(CMAKE_BUILD_TYPE STREQUAL "MinSizeRel")
  set(EXTERNAL_BUILD_TYPE "MinSizeRel")
else()
  set(EXTERNAL_BUILD_TYPE "Release")
endif()

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
get_filename_component(CHAINBLOCKS_DIR ${CMAKE_CURRENT_LIST_DIR} ABSOLUTE)

include(Sanitizers)
include(Platform)
include(Tidy)

# Rust build support
include(Rust)

# disable targets being added by CTestTargets.cmake
set_property(GLOBAL PROPERTY CTEST_TARGETS_ADDED 1)

enable_testing()
include(CTest)
include(deps/Catch2/extras/Catch.cmake)

add_subdirectory(deps)

add_subdirectory(src/core)
add_subdirectory(src/mal)

if(CHAINBLOCKS_WITH_EXTRA_BLOCKS)
  if(CHAINBLOCKS_WITH_RUST_BLOCKS)
    add_subdirectory(rust)
  endif()
  
  add_subdirectory(src/extra)
endif()