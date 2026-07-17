cmake_minimum_required(VERSION 3.16)
set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/../../shards/shards_esp32")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp32-test)
