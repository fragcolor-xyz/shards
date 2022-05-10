cmake_minimum_required(VERSION 3.17.5)
project("chainblocks tools")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin)

get_filename_component(CHAINBLOCKS_DIR ${CMAKE_CURRENT_LIST_DIR}/../.. ABSOLUTE)

add_subdirectory(${CHAINBLOCKS_DIR}/deps/spdlog deps/spdlog)
add_subdirectory(bin2c)
