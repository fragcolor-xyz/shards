project("chainblocks")
cmake_minimum_required(VERSION 3.14)

set(CMAKE_CXX_STANDARD 17)

### CLANG SANITIZERS (mac / linux only)
# Build Types
set(CMAKE_BUILD_TYPE ${CMAKE_BUILD_TYPE}
    CACHE STRING "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel tsan asan lsan msan ubsan"
    FORCE)

# ThreadSanitizer
set(CMAKE_C_FLAGS_TSAN
    "-fsanitize=thread -g -O1"
    CACHE STRING "Flags used by the C compiler during ThreadSanitizer builds."
    FORCE)
set(CMAKE_CXX_FLAGS_TSAN
    "-fsanitize=thread -g -O1"
    CACHE STRING "Flags used by the C++ compiler during ThreadSanitizer builds."
    FORCE)

# AddressSanitize
set(CMAKE_C_FLAGS_ASAN
    "-fsanitize=address -fno-optimize-sibling-calls -fsanitize-address-use-after-scope -fno-omit-frame-pointer -g -O1"
    CACHE STRING "Flags used by the C compiler during AddressSanitizer builds."
    FORCE)
set(CMAKE_CXX_FLAGS_ASAN
    "-fsanitize=address -fno-optimize-sibling-calls -fsanitize-address-use-after-scope -fno-omit-frame-pointer -g -O1"
    CACHE STRING "Flags used by the C++ compiler during AddressSanitizer builds."
    FORCE)

# LeakSanitizer
set(CMAKE_C_FLAGS_LSAN
    "-fsanitize=leak -fno-omit-frame-pointer -g -O1"
    CACHE STRING "Flags used by the C compiler during LeakSanitizer builds."
    FORCE)
set(CMAKE_CXX_FLAGS_LSAN
    "-fsanitize=leak -fno-omit-frame-pointer -g -O1"
    CACHE STRING "Flags used by the C++ compiler during LeakSanitizer builds."
    FORCE)

# MemorySanitizer
set(CMAKE_C_FLAGS_MSAN
    "-fsanitize=memory -fno-optimize-sibling-calls -fsanitize-memory-track-origins=2 -fno-omit-frame-pointer -g -O2"
    CACHE STRING "Flags used by the C compiler during MemorySanitizer builds."
    FORCE)
set(CMAKE_CXX_FLAGS_MSAN
    "-fsanitize=memory -fno-optimize-sibling-calls -fsanitize-memory-track-origins=2 -fno-omit-frame-pointer -g -O2"
    CACHE STRING "Flags used by the C++ compiler during MemorySanitizer builds."
    FORCE)

# UndefinedBehaviour
set(CMAKE_C_FLAGS_UBSAN
    "-fsanitize=undefined"
    CACHE STRING "Flags used by the C compiler during UndefinedBehaviourSanitizer builds."
    FORCE)
set(CMAKE_CXX_FLAGS_UBSAN
    "-fsanitize=undefined"
    CACHE STRING "Flags used by the C++ compiler during UndefinedBehaviourSanitizer builds."
    FORCE)
### SANITIZERS

if (NOT DEFINED CHAINBLOCKS_DIR)
  set(CHAINBLOCKS_DIR "${CMAKE_CURRENT_LIST_DIR}")
  set(BASE_TESTING 1)
endif()

### general
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  if(NOT APPLE)
    add_link_options(-fuse-ld=lld)
  endif()
elseif(WIN32)
  add_link_options(-static)
  add_compile_options(-static)
endif()

add_compile_options(-fno-threadsafe-statics -ffast-math -Wno-multichar)

if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
  if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    add_compile_options(-mllvm -inline-threshold=100000)
  elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    add_compile_options(-finline-limit=100000)
  elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Intel")
  # using Intel C++
  elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
  # using Visual Studio C++
  endif()
endif()
###

### libs
if(WIN32)
  set(BOOST_LIBS -lboost_context-mt -lboost_filesystem-mt)
  set(EXTRA_LIBS -lws2_32)
elseif(APPLE)
  set(BOOST_LIBS -lboost_context-mt -lboost_filesystem-mt)
  include_directories(/usr/local/include /usr/local/opt/readline/include)
  link_directories(/usr/local/lib /usr/local/opt/readline/lib)
else()
  set(BOOST_LIBS -lboost_context -lboost_filesystem)
  set(EXTRA_LIBS -pthread -ldl -lrt)
endif()

if(APPLE)
  add_compile_definitions(BOOST_STACKTRACE_GNU_SOURCE_NOT_REQUIRED)
endif()
###

#### Files to format and tidy
set(
  MY_PROJECT_SOURCE_FILES
  ${MY_PROJECT_SOURCE_FILES}
  ${CHAINBLOCKS_DIR}/src/core/runtime.cpp
  ${CHAINBLOCKS_DIR}/src/core/runtime.hpp
  # ${CHAINBLOCKS_DIR}/src/core/runtime_macros.hpp
  ${CHAINBLOCKS_DIR}/src/core/chainblocks.hpp
  # ${CHAINBLOCKS_DIR}/src/core/chainblocks.h
  ${CHAINBLOCKS_DIR}/src/core/ops.hpp
  # ${CHAINBLOCKS_DIR}/src/core/blocks_macros.hpp
  ${CHAINBLOCKS_DIR}/src/core/blocks/assert.cpp
  ${CHAINBLOCKS_DIR}/src/core/blocks/chains.cpp
  ${CHAINBLOCKS_DIR}/src/core/blocks/logging.cpp
  ${CHAINBLOCKS_DIR}/src/core/blocks/process.cpp
  ${CHAINBLOCKS_DIR}/src/core/blocks/seqs.cpp
  ${CHAINBLOCKS_DIR}/src/core/blocks/shared.hpp
  ${CHAINBLOCKS_DIR}/src/core/blocks/flow.cpp
  ${CHAINBLOCKS_DIR}/src/core/blocks/casting.cpp
  ${CHAINBLOCKS_DIR}/src/core/blocks/core.hpp
  ${CHAINBLOCKS_DIR}/src/core/blocks/core.cpp
  ${CHAINBLOCKS_DIR}/src/core/blocks/math.hpp
  ${CHAINBLOCKS_DIR}/src/mal/CBCore.cpp
  ${CHAINBLOCKS_DIR}/src/mal/CBGenerator.cpp
)

#### Header paths for tidy
set(
  MY_PROJECT_HEADER_PATHS
  ${MY_PROJECT_HEADER_PATHS}
  -I${CHAINBLOCKS_DIR}/src/core
)

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
endif()

#### Format target
if(CLANG_FORMAT_EXE)
  add_custom_target(
    format
    COMMAND ${CLANG_FORMAT_EXE} -i -style=LLVM ${MY_PROJECT_SOURCE_FILES}
  )
endif()
###

### setup clang tidy
find_program(
  CLANG_TIDY_EXE
  NAMES "clang-tidy"
  DOC "Path to clang-tidy executable"
  )
if(NOT CLANG_TIDY_EXE)
  message(STATUS "clang-tidy not found.")
else()
  message(STATUS "clang-tidy found: ${CLANG_TIDY_EXE}")
endif()

#### check target
if(CLANG_TIDY_EXE)
  add_custom_target(
    tidycheck
    COMMAND ${CLANG_TIDY_EXE} -checks=-*,clang-analyzer-*,performance-*,bugprone-* -fix ${MY_PROJECT_SOURCE_FILES} -- -std=c++17 -DDEBUG ${MY_PROJECT_HEADER_PATHS}
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
  ${CHAINBLOCKS_DIR}/src/core/blocks/assert.cpp
  ${CHAINBLOCKS_DIR}/src/core/blocks/chains.cpp
  ${CHAINBLOCKS_DIR}/src/core/blocks/logging.cpp
  ${CHAINBLOCKS_DIR}/src/core/blocks/flow.cpp
  ${CHAINBLOCKS_DIR}/src/core/blocks/process.cpp
  ${CHAINBLOCKS_DIR}/src/core/blocks/seqs.cpp
  ${CHAINBLOCKS_DIR}/src/core/blocks/casting.cpp
  ${CHAINBLOCKS_DIR}/src/core/blocks/core.cpp
  ${CHAINBLOCKS_DIR}/src/core/3rdparty/easylogging++.cc
)
add_dependencies(cbruntime format)

set(CB_RUNTIME_LIBS cbruntime)
###

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
  # 64 bits
  add_compile_definitions(AMD64_BUILD)
elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
  # 32 bits
  add_compile_definitions(I386_BUILD)
endif()

### mal blocks code generator
macro(lisp_builtins LIB GENNAME)
  add_executable(
    malgen_${GENNAME}
    "${CHAINBLOCKS_DIR}/src/mal/CBGenerator.cpp"
  )
  target_link_libraries(malgen_${GENNAME} ${CB_RUNTIME_LIBS} ${LIB} ${BOOST_LIBS} ${EXTRA_LIBS} ${ARGN})

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

lisp_builtins(cbruntime core_generator)

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
target_link_libraries(cbl -lreadline -lncurses ${CB_RUNTIME_LIBS} ${BOOST_LIBS} ${EXTRA_LIBS})
###