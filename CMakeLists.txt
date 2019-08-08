project("chainblocks")
cmake_minimum_required(VERSION 3.15)

set(CMAKE_CXX_STANDARD 17)

if (NOT DEFINED CHAINBLOCKS_DIR)
  set(CHAINBLOCKS_DIR "${CMAKE_CURRENT_LIST_DIR}")
  set(BASE_TESTING 1)
endif()

### general
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  # add_link_options(-static -fuse-ld=lld)
elseif(WIN32)
  add_link_options(-static -lwinpthreads)
  add_compile_options(-static)
endif()

add_compile_options(-fno-threadsafe-statics -ffast-math -Wno-multichar)
###

### boost context
if(WIN32)
  set(BOOST_LIBS -lboost_context-mt -lboost_filesystem-mt)
  set(EXTRA_LIBS -lws2_32)
else()
  set(BOOST_LIBS -lboost_context -lboost_filesystem)
  set(EXTRA_LIBS -pthread -ldl -lrt)
endif()
###

### setup clang format
find_program(
  CLANG_FORMAT_EXE
  NAMES "clang-format"
  DOC "Path to clang-format executable"
  )
if(NOT CLANG_FORMAT_EXE)
  message(STATUS "clang-format not found.")
else()
  message(STATUS "clang-format found: ${CLANG_FORMAT_EXE}")
  set(DO_CLANG_FORMAT "${CLANG_FORMAT_EXE}" "-i -style=LLVM")
endif()

#### Files to format
set(
  MY_PROJECT_SOURCE_FILES
  ${MY_PROJECT_SOURCE_FILES}
  ${CHAINBLOCKS_DIR}/src/core/runtime.cpp
  ${CHAINBLOCKS_DIR}/src/core/runtime.hpp
  ${CHAINBLOCKS_DIR}/src/core/runtime_macros.hpp
  ${CHAINBLOCKS_DIR}/src/core/chainblocks.hpp
  ${CHAINBLOCKS_DIR}/src/core/chainblocks.h
  ${CHAINBLOCKS_DIR}/src/core/blocks_macros.hpp
  ${CHAINBLOCKS_DIR}/src/core/blocks/assert.hpp
  ${CHAINBLOCKS_DIR}/src/core/blocks/chains.hpp
  ${CHAINBLOCKS_DIR}/src/core/blocks/logging.hpp
  ${CHAINBLOCKS_DIR}/src/core/blocks/process.hpp
  ${CHAINBLOCKS_DIR}/src/core/blocks/seqs.hpp
  ${CHAINBLOCKS_DIR}/src/core/blocks/shared.hpp
  ${CHAINBLOCKS_DIR}/src/core/blocks/tables.hpp
  ${CHAINBLOCKS_DIR}/src/mal/CBCore.cpp
  ${CHAINBLOCKS_DIR}/src/mal/CBGenerator.cpp
)

#### Format target
function(prepend var prefix)
  set(listVar "")

  foreach(f ${ARGN})
    list(APPEND listVar "${prefix}/${f}")
  endforeach()

  set(${var} "${listVar}" PARENT_SCOPE)
endfunction()

if(CLANG_FORMAT_EXE)
  # prepend(FILES_TO_FORMAT ${CMAKE_CURRENT_SOURCE_DIR} ${MY_PROJECT_SOURCE_FILES})

  add_custom_target(
    format
    COMMAND ${CLANG_FORMAT_EXE} -i -style=LLVM ${MY_PROJECT_SOURCE_FILES}
  )
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
add_dependencies(cbruntime format)
###

### nim stuff
find_program(NIM_EXECUTABLE nim PATHS ENV PATH)

set(NIM_OPTIONS cpp -d:cmake --newruntime -d:useWinAnsi -d:useMalloc -d:noSignalHandler --stackTrace:off --lineTrace:off)

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
macro(lisp_builtins LIB GENNAME)
  add_executable(
    malgen_${GENNAME}
    "${CHAINBLOCKS_DIR}/src/mal/CBGenerator.cpp"
  )
  target_link_libraries(malgen_${GENNAME} cbruntime ${LIB} ${BOOST_LIBS} ${EXTRA_LIBS} ${ARGN})

  # https://stackoverflow.com/questions/31274577/custom-target-as-a-target-library-in-cmake
  add_custom_command(
    OUTPUT CBGenerated_${GENNAME}.hpp
    COMMAND
    malgen_${GENNAME}
    DEPENDS
    malgen_${GENNAME}
  )
  add_custom_target(${GENNAME}_target DEPENDS CBGenerated_${GENNAME}.hpp)
  add_library(${GENNAME} STATIC IMPORTED GLOBAL)
  add_dependencies(${GENNAME} ${GENNAME}_target)
  set_target_properties(${GENNAME} PROPERTIES
    IMPORTED_LOCATION ${CMAKE_CURRENT_BINARY_DIR}/CBGenerated_${GENNAME}.hpp
  )
endmacro()
###

lisp_builtins(cbnim_lib core_generator)

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
add_dependencies(cbl core_generator)
include_directories(${CMAKE_CURRENT_BINARY_DIR})
set_property(TARGET cbl PROPERTY COMPILE_FLAGS "-DHAS_CB_GENERATED")
target_link_libraries(cbl -lreadline -lncurses cbruntime cbnim_lib ${BOOST_LIBS} ${EXTRA_LIBS})
###