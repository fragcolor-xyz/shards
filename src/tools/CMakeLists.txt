cmake_minimum_required(VERSION 3.17.5)
project("shards tools")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin)

get_filename_component(SHARDS_DIR ${CMAKE_CURRENT_LIST_DIR}/../.. ABSOLUTE)

add_subdirectory(${SHARDS_DIR}/deps/spdlog deps/spdlog)
add_subdirectory(bin2c)
