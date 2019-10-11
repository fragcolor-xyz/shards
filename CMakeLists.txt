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

add_compile_definitions(ELPP_FEATURE_PERFORMANCE_TRACKING ELPP_DEFAULT_LOG_FILE="chainblocks.log")

### general
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  if(WIN32)
    add_link_options(-fuse-ld=lld)
    # need those for custom new/delete
    add_link_options(-static-libstdc++)
    add_compile_options(-static-libstdc++)
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
  ${CHAINBLOCKS_DIR}/src/core/blocks_macros.hpp
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
  ${CHAINBLOCKS_DIR}/src/core/blocks/linalg.cpp
  ${CHAINBLOCKS_DIR}/src/mal/CBCore.cpp
  ${CHAINBLOCKS_DIR}/src/mal/CBGenerator.cpp
  ${CHAINBLOCKS_DIR}/src/core/blocks/serialization.cpp
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
    COMMAND ${CLANG_TIDY_EXE} -checks=-*,clang-analyzer-*,performance-*,bugprone-* -fix ${MY_PROJECT_SOURCE_FILES} -- -std=c++11 -Wno-c++17-extensions -DDEBUG ${MY_PROJECT_HEADER_PATHS}
  )
endif()
###

if(UNIX AND NOT APPLE)
  set(LINUX TRUE)
endif()

### runtime
include_directories(
  ${CHAINBLOCKS_DIR}/deps/rpmalloc
  ${CHAINBLOCKS_DIR}/deps/easyloggingpp/src
  ${CHAINBLOCKS_DIR}/deps/json/include
  ${CHAINBLOCKS_DIR}/deps/parallel-hashmap
  ${CHAINBLOCKS_DIR}/deps/SPSCQueue/include
  ${CHAINBLOCKS_DIR}/deps/stb
  ${CHAINBLOCKS_DIR}/deps/xxHash
  ${CHAINBLOCKS_DIR}/src/core
  ${CHAINBLOCKS_DIR}/src/core/3rdparty
)

if(NOT BUILD_CORE_ONLY)
  add_compile_definitions(CB_WITH_EXTRAS)

  include_directories(
    ${CHAINBLOCKS_DIR}/deps/bx/include
    ${CHAINBLOCKS_DIR}/deps/bx/3rdparty
    ${CHAINBLOCKS_DIR}/deps/bgfx/include
    ${CHAINBLOCKS_DIR}/deps/bgfx/3rdparty
    ${CHAINBLOCKS_DIR}/deps/bgfx/examples/common/imgui
    ${CHAINBLOCKS_DIR}/deps/bimg/include
  )

  if(WIN32)
    add_compile_definitions(BGFX_CONFIG_RENDERER_DIRECT3D11)
    include_directories(
      ${CHAINBLOCKS_DIR}/deps/bx/include/compat/mingw
      ${CHAINBLOCKS_DIR}/external/SDL2-2.0.10/x86_64-w64-mingw32/include/SDL2
    )
    set(PLATFORM_SOURCES 
      ${CHAINBLOCKS_DIR}/deps/bgfx/src/amalgamated.cpp
    )

    set(CBLP_PLATFORM_LIBS
      -lOle32 -lImm32 -lWinmm -lGdi32 -lVersion -lOleAut32 -lSetupAPI -lPsapi -lD3D11 -lDXGI
    )
  elseif(APPLE)
    add_compile_definitions(BGFX_CONFIG_RENDERER_METAL)
    include_directories(
      ${CHAINBLOCKS_DIR}/deps/bx/include/compat/osx
      ${CHAINBLOCKS_DIR}/external/SDL2-2.0.10/include
    )
    set(PLATFORM_SOURCES
      ${CHAINBLOCKS_DIR}/deps/bgfx/src/amalgamated.mm
    )

    set(CBLP_PLATFORM_LIBS
      ${CHAINBLOCKS_DIR}/deps/bimg/.build/osx64_clang/bin/libbimgRelease.a
      ${CHAINBLOCKS_DIR}/external/SDL2-2.0.10/build/libSDL2.a
      -liconv
      "-framework Foundation"
      "-framework Cocoa"
      "-framework CoreAudio"
      "-framework AudioToolbox"
      "-framework CoreVideo"
      "-framework ForceFeedback"
      "-framework IOKit"
      "-framework Carbon"
      "-framework QuartzCore"
      "-framework Metal"
    )
  elseif(LINUX)
    add_compile_definitions(BGFX_CONFIG_RENDERER_OPENGL)
    include_directories(
      /usr/include/SDL2
      ${CHAINBLOCKS_DIR}/deps/bgfx/3rdparty/khronos
    )
    set(PLATFORM_SOURCES
      ${CHAINBLOCKS_DIR}/deps/bgfx/src/amalgamated.cpp
    )

    set(CBLP_PLATFORM_LIBS
      ${CMAKE_CURRENT_LIST_DIR}/deps/bimg/.build/linux64_gcc/bin/libbimgRelease.a
      -lSDL2
      -lX11
      -lGL
    )
  endif()

  set(CB_EXTRAS
    ${CHAINBLOCKS_DIR}/src/extra/runtime.cpp
    ${CHAINBLOCKS_DIR}/src/extra/bgfx.cpp
    ${CHAINBLOCKS_DIR}/src/extra/imgui.cpp
    ${CHAINBLOCKS_DIR}/deps/bx/src/amalgamated.cpp
    ${CHAINBLOCKS_DIR}/deps/bgfx/examples/common/imgui/imgui.cpp
    ${CHAINBLOCKS_DIR}/deps/bgfx/3rdparty/dear-imgui/imgui.cpp
    ${CHAINBLOCKS_DIR}/deps/bgfx/3rdparty/dear-imgui/imgui_draw.cpp
    ${CHAINBLOCKS_DIR}/deps/bgfx/3rdparty/dear-imgui/imgui_widgets.cpp
    ${PLATFORM_SOURCES}
  )
endif()

add_library(
  cbruntime STATIC
  ${CHAINBLOCKS_DIR}/deps/rpmalloc/rpmalloc/rpmalloc.c
  ${CHAINBLOCKS_DIR}/deps/easyloggingpp/src/easylogging++.cc
  ${CHAINBLOCKS_DIR}/src/core/runtime.cpp
  ${CHAINBLOCKS_DIR}/src/core/blocks/assert.cpp
  ${CHAINBLOCKS_DIR}/src/core/blocks/chains.cpp
  ${CHAINBLOCKS_DIR}/src/core/blocks/logging.cpp
  ${CHAINBLOCKS_DIR}/src/core/blocks/flow.cpp
  ${CHAINBLOCKS_DIR}/src/core/blocks/process.cpp
  ${CHAINBLOCKS_DIR}/src/core/blocks/seqs.cpp
  ${CHAINBLOCKS_DIR}/src/core/blocks/casting.cpp
  ${CHAINBLOCKS_DIR}/src/core/blocks/core.cpp
  ${CHAINBLOCKS_DIR}/src/core/blocks/linalg.cpp
  ${CHAINBLOCKS_DIR}/src/core/blocks/serialization.cpp
  ${CB_EXTRAS}
)
set_property(TARGET cbruntime PROPERTY COMPILE_FLAGS "-DCB_DLL_EXPORT")
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

lisp_builtins(cbruntime core_generator ${CBLP_PLATFORM_LIBS})

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
set_property(TARGET cbl PROPERTY COMPILE_FLAGS "-DHAS_CB_GENERATED -DCB_DLL_EXPORT")
target_link_libraries(cbl -lreadline -lncurses ${CB_RUNTIME_LIBS} ${BOOST_LIBS} ${EXTRA_LIBS} ${CBLP_PLATFORM_LIBS})
###