project("chainblocks")
cmake_minimum_required(VERSION 3.15)

set(CMAKE_CXX_STANDARD 17)

if (NOT DEFINED CHAINBLOCKS_DIR)
  set(CHAINBLOCKS_DIR "${CMAKE_CURRENT_LIST_DIR}")
  set(BASE_TESTING 1)
endif()

### general
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  # set(LINKING_OPTIONS -fuse-ld=lld)
endif()
###

### boost context
if(WIN32)
  set(BOOST_CONTEXT -lboost_context-mt)
  set(EXTRA_LIBS "")
else()
  set(BOOST_CONTEXT -lboost_context)
  set(EXTRA_LIBS -lrt)
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

### nim stuff
find_program(NIM_EXECUTABLE nim PATHS ENV PATH)

set(NIM_OPTIONS cpp -d:cmake --newruntime -d:useWinAnsi -d:useMalloc)

if(DEFINED NIMBLE_PATH)
  set(NIM_OPTIONS ${NIM_OPTIONS} --NimblePath:${NIMBLE_PATH})
endif()

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

### corecb via nim
# https://stackoverflow.com/questions/31274577/custom-target-as-a-target-library-in-cmake
add_custom_command(
  OUTPUT libnimcb.a
  COMMAND
    ${NIM_EXECUTABLE} ${NIM_OPTIONS} -f --app:staticlib --noMain --nimcache:nimcache-base-cbjs -o:libnimcb.a -p:${CMAKE_CURRENT_LIST_DIR}/src/nim --passL:-static --passC:-static --passC:-DCHAINBLOCKS_RUNTIME ${CHAINBLOCKS_DIR}/src/nim/chainblocks.nim
  DEPENDS
    ${nim_files_list}
)
add_custom_target(cbnim_lib_target DEPENDS libnimcb.a)
add_library(cbnim_lib STATIC IMPORTED GLOBAL)
add_dependencies(cbnim_lib cbnim_lib_target)
set_target_properties(cbnim_lib PROPERTIES
  IMPORTED_LOCATION ${CMAKE_CURRENT_BINARY_DIR}/libnimcb.a
)
###

### mal blocks code generator
add_executable(
  malgen
  "${CHAINBLOCKS_DIR}/src/mal/CBGenerator.cpp"
)
set_property(TARGET malgen PROPERTY COMPILE_FLAGS "-static")
target_link_libraries(malgen cbruntime cbnim_lib ${BOOST_CONTEXT} ${EXTRA_LIBS})

# https://stackoverflow.com/questions/31274577/custom-target-as-a-target-library-in-cmake
add_custom_command(
  OUTPUT CBGenerated.hpp
  COMMAND
    malgen
  DEPENDS
    malgen
)
add_custom_target(generator_target DEPENDS CBGenerated.hpp)
add_library(generator STATIC IMPORTED GLOBAL)
add_dependencies(generator generator_target)
set_target_properties(generator PROPERTIES
  IMPORTED_LOCATION ${CMAKE_CURRENT_BINARY_DIR}/CBGenerated.hpp
)
###

### mal
add_executable(
  cbl
  "${CHAINBLOCKS_DIR}/src/mal/stepA_mal.cpp"
  "${CHAINBLOCKS_DIR}/src/mal/Core.cpp"
  "${CHAINBLOCKS_DIR}/src/mal/CBCore.cpp"
  "${CHAINBLOCKS_DIR}/src/mal/Environment.cpp"
  "${CHAINBLOCKS_DIR}/src/mal/Reader.cpp"
  "${CHAINBLOCKS_DIR}/src/mal/ReadLine.cpp"
  "${CHAINBLOCKS_DIR}/src/mal/String.cpp"
  "${CHAINBLOCKS_DIR}/src/mal/Types.cpp"
  "${CHAINBLOCKS_DIR}/src/mal/Validation.cpp"
  "${CHAINBLOCKS_DIR}/src/mal/CBCore.cpp"
)
add_dependencies(cbl generator)
include_directories(${CMAKE_CURRENT_BINARY_DIR})
set_property(TARGET cbl PROPERTY COMPILE_FLAGS "-static -ffast-math -DHAS_CB_GENERATED")
target_link_libraries(cbl -lreadline cbruntime cbnim_lib ${BOOST_CONTEXT} ${EXTRA_LIBS})
###