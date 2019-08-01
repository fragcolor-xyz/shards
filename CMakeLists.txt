project("chainblocks")
cmake_minimum_required(VERSION 3.15)

if (NOT DEFINED CHAINBLOCKS_DIR)
  set(CHAINBLOCKS_DIR "${CMAKE_CURRENT_LIST_DIR}")
  set(BASE_TESTING 1)
endif()

### general
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  # set(LINKING_OPTIONS -fuse-ld=lld)
endif()
###

### runtime
include_directories(
  ${CHAINBLOCKS_DIR}/src/core
  ${CHAINBLOCKS_DIR}/src/core/3rdparty
)

add_library(
  cbruntime STATIC
  ${CHAINBLOCKS_DIR}/src/core/runtime.cpp
  ${CHAINBLOCKS_DIR}/src/core/3rdparty/easylogging++.cc
)
set_property(TARGET cbruntime PROPERTY COMPILE_FLAGS "-DELPP_FEATURE_CRASH_LOG -static -ffast-math")
###

### quickjs
file(STRINGS ${CHAINBLOCKS_DIR}/../quicknimjs/src/quicknimjspkg/quickjs/VERSION quickjs_version)
add_library(
  quickjs STATIC
  ${CHAINBLOCKS_DIR}/../quicknimjs/src/quicknimjspkg/quickjs/quickjs.c
  ${CHAINBLOCKS_DIR}/../quicknimjs/src/quicknimjspkg/quickjs/quickjs-libc.c
  ${CHAINBLOCKS_DIR}/../quicknimjs/src/quicknimjspkg/quickjs/cutils.c
  ${CHAINBLOCKS_DIR}/../quicknimjs/src/quicknimjspkg/quickjs/libunicode.c
  ${CHAINBLOCKS_DIR}/../quicknimjs/src/quicknimjspkg/quickjs/libregexp.c
  ${CHAINBLOCKS_DIR}/../quicknimjs/src/quicknimjspkg/quickjs/libbf.c
)
set_property(TARGET quickjs PROPERTY COMPILE_FLAGS "-DCONFIG_VERSION=\\\"${quickjs_version}\\\" -D_GNU_SOURCE -static -ffast-math")
###

### nim stuff
find_program(NIM_EXECUTABLE nim PATHS ENV PATH)

set(NIM_OPTIONS cpp -d:cmake --newruntime -d:useWinAnsi -d:useMalloc)

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
  # 64 bits
  set(NIM_OPTIONS ${NIM_OPTIONS} --cpu:amd64)
elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
  # 32 bits
  set(NIM_OPTIONS ${NIM_OPTIONS} --cpu:i386)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(NIM_OPTIONS ${NIM_OPTIONS} --passC:-ggdb)
elseif(CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
  set(NIM_OPTIONS ${NIM_OPTIONS} -d:danger --passC:-ggdb)
else()
  set(NIM_OPTIONS ${NIM_OPTIONS} -d:danger)
endif()

# list(APPEND CMAKE_CXX_SOURCE_FILE_EXTENSIONS nim)

file(GLOB_RECURSE nim_files ${CHAINBLOCKS_DIR}/../*.nim)
foreach(nim_file ${nim_files})
  list(APPEND nim_files_list "${nim_file}")
endforeach()
###

### cbjs.exe via nim
# https://stackoverflow.com/questions/31274577/custom-target-as-a-target-library-in-cmake
add_custom_command(
  OUTPUT libcbjs.a
  COMMAND
    ${NIM_EXECUTABLE} ${NIM_OPTIONS} -f --app:staticlib --nimcache:nimcache-base-cbjs -o:libcbjs.a -p:${CMAKE_CURRENT_LIST_DIR}/src/nim --passL:-static --passC:-static --passC:-DCHAINBLOCKS_RUNTIME ${CHAINBLOCKS_DIR}/src/js/cbjs.nim
  DEPENDS
    ${nim_files_list}
)
add_custom_target(cbjs_lib_target DEPENDS libcbjs.a)
add_library(cbjs_lib STATIC IMPORTED GLOBAL)
add_dependencies(cbjs_lib cbjs_lib_target)
set_target_properties(cbjs_lib PROPERTIES
  IMPORTED_LOCATION ${CMAKE_CURRENT_BINARY_DIR}/libcbjs.a
)

# Need this little hack
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/null.cpp "")
add_executable(cbjs ${CMAKE_CURRENT_BINARY_DIR}/null.cpp)
if(WIN32)
  set_property(TARGET cbjs PROPERTY LINK_FLAGS "-static ${LINKING_OPTIONS}")
  set(BOOST_CONTEXT -lboost_context-mt)
else()
  set_property(TARGET cbjs PROPERTY LINK_FLAGS "-static ${LINKING_OPTIONS}")
  set(BOOST_CONTEXT -lboost_context)
endif()
target_link_libraries(
  cbjs
  cbjs_lib
  cbruntime
  quickjs
  ${BOOST_CONTEXT}
)
###