# remember to use CMAKE_EXPORT_COMPILE_COMMAND=1 with Ninja, so clang tools work out of the box
# you need to copy the .json file in the root directory
# also CMAKE_BUILD_TYPE=Debug or so

project("chainblocks")
cmake_minimum_required(VERSION 3.14)

set(CMAKE_CXX_STANDARD 17)

if (CMAKE_SYSTEM_PROCESSOR MATCHES "(x86)|(X86)|(amd64)|(AMD64)")
  set (X86 TRUE)
else ()
  set (X86 FALSE)
endif ()

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

if (NOT DEFINED CHAINBLOCKS_EXTRA_LINKS)
  set(CHAINBLOCKS_EXTRA_LINKS "")
endif()

if(X86)
  add_compile_definitions(ELPP_FEATURE_PERFORMANCE_TRACKING ELPP_DEFAULT_LOG_FILE="chainblocks.log" BOOST_INTERPROCESS_BOOTSTAMP_IS_LASTBOOTUPTIME=1)
  add_compile_options(-Wall)
else()
  add_compile_definitions(ELPP_FEATURE_PERFORMANCE_TRACKING ELPP_DEFAULT_LOG_FILE="chainblocks.log" BOOST_INTERPROCESS_BOOTSTAMP_IS_LASTBOOTUPTIME=1)
  add_compile_options(-Wall)
endif()

### general
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  if(WIN32)
    add_link_options(-fuse-ld=lld)
  endif()
elseif(WIN32)
  add_link_options(-static)
endif()

add_compile_options(-fno-threadsafe-statics -ffast-math -funroll-loops -Wno-multichar)

if(X86)
  if(NOT FORCE_CORE2)
    # CBVars are m256 basically, so this is extremely good! (AVX!)
    add_compile_options(-march=sandybridge)
  else()
    # Not as good as AVX but CBVarPayload being m128 and such rocks it!
    add_compile_options(-march=core2)
  endif()
endif()

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
  # 64 bits
  add_compile_definitions(CPUBITS64)
elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
  # 32 bits
  add_compile_definitions(CPUBITS32)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Release")
  # custom allocator
  add_compile_definitions(USE_RPMALLOC)

  # aggressive inlining
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
  if(NOT IOS)
    set(BOOST_LIBS -lboost_context-mt)
  endif()
  include_directories(/usr/local/include /usr/local/opt/readline/include)
  link_directories(/usr/local/lib /usr/local/opt/readline/lib)
else()
  set(BOOST_LIBS -lboost_context)
  set(EXTRA_LIBS -pthread -ldl -lrt)
endif()

if(APPLE)
  add_compile_definitions(BOOST_STACKTRACE_GNU_SOURCE_NOT_REQUIRED)
  add_compile_options(-Wextra -Wno-unused-parameter -Wno-missing-field-initializers)
endif()
###

#### Files to format and tidy
set(
  MY_PROJECT_SOURCE_FILES
  ${MY_PROJECT_SOURCE_FILES}
  ${CHAINBLOCKS_DIR}/include/blockwrapper.hpp
  ${CHAINBLOCKS_DIR}/include/dllblock.hpp
  ${CHAINBLOCKS_DIR}/include/stbpp.hpp
  ${CHAINBLOCKS_DIR}/include/utility.hpp
  ${CHAINBLOCKS_DIR}/include/asiotools.hpp
  ${CHAINBLOCKS_DIR}/include/chainblocks.hpp
  ${CHAINBLOCKS_DIR}/include/chainblocks.h
  ${CHAINBLOCKS_DIR}/include/ops.hpp
  ${CHAINBLOCKS_DIR}/src/core/runtime.cpp
  ${CHAINBLOCKS_DIR}/src/core/runtime.hpp
  ${CHAINBLOCKS_DIR}/src/core/core.hpp
  ${CHAINBLOCKS_DIR}/src/core/ops_internal.hpp
  ${CHAINBLOCKS_DIR}/src/core/blocks_macros.hpp
  ${CHAINBLOCKS_DIR}/src/core/blocks/process.cpp
  ${CHAINBLOCKS_DIR}/src/core/blocks/assert.cpp
  ${CHAINBLOCKS_DIR}/src/core/blocks/chains.cpp
  ${CHAINBLOCKS_DIR}/src/core/blocks/logging.cpp
  ${CHAINBLOCKS_DIR}/src/core/blocks/seqs.cpp
  ${CHAINBLOCKS_DIR}/src/core/blocks/shared.hpp
  ${CHAINBLOCKS_DIR}/src/core/blocks/flow.cpp
  ${CHAINBLOCKS_DIR}/src/core/blocks/casting.cpp
  ${CHAINBLOCKS_DIR}/src/core/blocks/core.hpp
  ${CHAINBLOCKS_DIR}/src/core/blocks/core.cpp
  ${CHAINBLOCKS_DIR}/src/core/blocks/math.hpp
  ${CHAINBLOCKS_DIR}/src/core/blocks/linalg.cpp
  ${CHAINBLOCKS_DIR}/src/core/blocks/fs.cpp
  ${CHAINBLOCKS_DIR}/src/core/blocks/json.cpp
  ${CHAINBLOCKS_DIR}/src/core/blocks/network.cpp
  ${CHAINBLOCKS_DIR}/src/core/blocks/struct.cpp
  ${CHAINBLOCKS_DIR}/src/core/blocks/channels.cpp
  ${CHAINBLOCKS_DIR}/src/extra/bgfx.cpp
  ${CHAINBLOCKS_DIR}/src/extra/bgfx.hpp
  ${CHAINBLOCKS_DIR}/src/extra/imgui.cpp
  ${CHAINBLOCKS_DIR}/src/extra/imgui.hpp
  ${CHAINBLOCKS_DIR}/src/extra/runtime.cpp
  ${CHAINBLOCKS_DIR}/src/extra/desktop.hpp
  ${CHAINBLOCKS_DIR}/src/extra/desktop.win.cpp
  ${CHAINBLOCKS_DIR}/src/extra/desktop.capture.win.hpp
  ${CHAINBLOCKS_DIR}/src/extra/snappy.cpp
  ${CHAINBLOCKS_DIR}/src/mal/CBCore.cpp
  ${CHAINBLOCKS_DIR}/src/core/blocks/serialization.cpp
  ${CHAINBLOCKS_DIR}/src/core/blocks/time.cpp
  ${CHAINBLOCKS_DIR}/src/core/blocks/os.cpp
  ${CHAINBLOCKS_DIR}/src/core/blocks/regex.cpp
  ${CHAINBLOCKS_DIR}/src/core/edn/print.hpp
  ${CHAINBLOCKS_DIR}/src/core/edn/read.hpp
  ${CHAINBLOCKS_DIR}/src/core/edn/eval.hpp
  ${CHAINBLOCKS_DIR}/src/core/edn/eval.cpp
  )

#### Header paths for tidy
set(
  MY_PROJECT_HEADER_PATHS
  ${MY_PROJECT_HEADER_PATHS}
  -I${CHAINBLOCKS_DIR}/include
  -I${CHAINBLOCKS_DIR}/src/core
  -I${CHAINBLOCKS_DIR}/deps/rpmalloc
  -I${CHAINBLOCKS_DIR}/deps/snappy
  -I${CHAINBLOCKS_DIR}/deps/easyloggingpp/src
  -I${CHAINBLOCKS_DIR}/deps/parallel-hashmap
  -I${CHAINBLOCKS_DIR}/deps/SPSCQueue/include
  -I${CHAINBLOCKS_DIR}/deps/stb
  -I${CHAINBLOCKS_DIR}/deps/kcp
  -I${CHAINBLOCKS_DIR}/deps/xxHash
  -I${CHAINBLOCKS_DIR}/deps/json/include
  -I${CHAINBLOCKS_DIR}/deps/nameof/include
  -I${CHAINBLOCKS_DIR}/deps/magic_enum/include
  -I${CHAINBLOCKS_DIR}/src/core
  -I${CHAINBLOCKS_DIR}/src/core/3rdparty
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

#### tidy target
set(tidyfied)
foreach(_file ${MY_PROJECT_SOURCE_FILES})
  if(CLANG_TIDY_EXE)
    add_custom_command(
      OUTPUT ${_file}.noop
      COMMAND ${CLANG_TIDY_EXE} -checks=-*,clang-analyzer-*,performance-*,bugprone-* -fix ${_file} -- -std=c++17 -DDEBUG ${MY_PROJECT_HEADER_PATHS}
      )
    list(APPEND tidyfied ${_file}.noop)
  endif()
endforeach()
add_custom_target(tidy DEPENDS ${tidyfied})
###

if(UNIX AND NOT APPLE)
  set(LINUX TRUE)
endif()

if(LINUX)
  add_link_options(-export-dynamic)
  if(USE_VALGRIND)
    add_compile_definitions(BOOST_USE_VALGRIND)
  endif()
endif()

### runtime
include_directories(
  ${CHAINBLOCKS_DIR}/deps/rpmalloc
  ${CHAINBLOCKS_DIR}/deps/snappy
  ${CHAINBLOCKS_DIR}/deps/easyloggingpp/src
  ${CHAINBLOCKS_DIR}/deps/parallel-hashmap
  ${CHAINBLOCKS_DIR}/deps/SPSCQueue/include
  ${CHAINBLOCKS_DIR}/deps/stb
  ${CHAINBLOCKS_DIR}/deps/xxHash
  ${CHAINBLOCKS_DIR}/deps/json/include
  ${CHAINBLOCKS_DIR}/deps/nameof/include
  ${CHAINBLOCKS_DIR}/deps/magic_enum/include
  ${CHAINBLOCKS_DIR}/deps/kcp
  ${CHAINBLOCKS_DIR}/include
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
    ${CHAINBLOCKS_DIR}/deps/imgui_club/imgui_memory_editor
    ${CHAINBLOCKS_DIR}/deps/bgfx/examples/common/imgui
    ${CHAINBLOCKS_DIR}/deps/bimg/include
    ${CHAINBLOCKS_DIR}/deps/snappy/build
    ${CHAINBLOCKS_DIR}/deps/snappy/build32
    )

  if(WIN32)
    add_compile_definitions(BGFX_CONFIG_RENDERER_DIRECT3D11)
    include_directories(
      ${CHAINBLOCKS_DIR}/deps/bx/include/compat/mingw
      ${CHAINBLOCKS_DIR}/external/SDL2-2.0.10/x86_64-w64-mingw32/include/SDL2
      )
    set(PLATFORM_SOURCES 
      ${CHAINBLOCKS_DIR}/deps/bgfx/src/amalgamated.cpp
      ${CHAINBLOCKS_DIR}/src/extra/desktop.win.cpp
      )

    set(CBLP_PLATFORM_LIBS
      -lntdll -lOle32 -lImm32 -lWinmm -lGdi32 -lVersion -lOleAut32 -lSetupAPI -lPsapi -lD3D11 -lDXGI
      )

    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
      # 64 bits
      set(CBLP_PLATFORM_LIBS
        ${CHAINBLOCKS_DIR}/deps/bimg/.build/win64_mingw-gcc/bin/libbimgRelease.a
        ${CHAINBLOCKS_DIR}/external/SDL2-2.0.10/x86_64-w64-mingw32/lib/libSDL2.a
	${CHAINBLOCKS_DIR}/deps/snappy/build/libsnappy.a
        ${CBLP_PLATFORM_LIBS}
	)
    elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
      # 32 bits
      set(CBLP_PLATFORM_LIBS
        ${CHAINBLOCKS_DIR}/deps/bimg/.build/win32_mingw-gcc/bin/libbimgRelease.a
        ${CHAINBLOCKS_DIR}/external/SDL2-2.0.10/i686-w64-mingw32/lib/libSDL2.a
	${CHAINBLOCKS_DIR}/deps/snappy/build32/libsnappy.a
        ${CBLP_PLATFORM_LIBS}
	)
    endif()
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
      ${CHAINBLOCKS_DIR}/deps/snappy/build/libsnappy.a
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
      ${CHAINBLOCKS_DIR}/deps/snappy/build/libsnappy.a
      -lSDL2
      -lX11
      -lGL
      )
  endif()

  set(CB_EXTRAS
    ${CHAINBLOCKS_DIR}/src/extra/runtime.cpp
    ${CHAINBLOCKS_DIR}/src/extra/bgfx.cpp
    ${CHAINBLOCKS_DIR}/src/extra/imgui.cpp
    ${CHAINBLOCKS_DIR}/src/extra/snappy.cpp
    ${CHAINBLOCKS_DIR}/deps/bx/src/amalgamated.cpp
    ${CHAINBLOCKS_DIR}/deps/bgfx/examples/common/imgui/imgui.cpp
    ${CHAINBLOCKS_DIR}/deps/bgfx/3rdparty/dear-imgui/imgui.cpp
    ${CHAINBLOCKS_DIR}/deps/bgfx/3rdparty/dear-imgui/imgui_draw.cpp
    ${CHAINBLOCKS_DIR}/deps/bgfx/3rdparty/dear-imgui/imgui_widgets.cpp
    ${PLATFORM_SOURCES}
    )
endif()

if(NOT IOS)
  set(CB_EXTRAS
    ${CB_EXTRAS}
    ${CHAINBLOCKS_DIR}/deps/rpmalloc/rpmalloc/rpmalloc.c
    )
else()
  set(CB_EXTRAS
    ${CB_EXTRAS}
    ${CHAINBLOCKS_DIR}/deps/boost-context/src/posix/stack_traits.cpp
    ${CHAINBLOCKS_DIR}/deps/boost-context/src/asm/jump_x86_64_sysv_macho_gas.S
    ${CHAINBLOCKS_DIR}/deps/boost-context/src/asm/make_x86_64_sysv_macho_gas.S
    ${CHAINBLOCKS_DIR}/deps/boost-context/src/asm/ontop_x86_64_sysv_macho_gas.S
    )
endif()

set(RUNTIME_SRC
  ${CHAINBLOCKS_DIR}/deps/easyloggingpp/src/easylogging++.cc
  ${CHAINBLOCKS_DIR}/src/core/runtime.cpp
  ${CHAINBLOCKS_DIR}/src/core/blocks/assert.cpp
  ${CHAINBLOCKS_DIR}/src/core/blocks/chains.cpp
  ${CHAINBLOCKS_DIR}/src/core/blocks/logging.cpp
  ${CHAINBLOCKS_DIR}/src/core/blocks/flow.cpp
  ${CHAINBLOCKS_DIR}/src/core/blocks/seqs.cpp
  ${CHAINBLOCKS_DIR}/src/core/blocks/casting.cpp
  ${CHAINBLOCKS_DIR}/src/core/blocks/core.cpp
  ${CHAINBLOCKS_DIR}/src/core/blocks/linalg.cpp
  ${CHAINBLOCKS_DIR}/src/core/blocks/serialization.cpp
  ${CHAINBLOCKS_DIR}/src/core/blocks/fs.cpp
  ${CHAINBLOCKS_DIR}/src/core/blocks/json.cpp
  ${CHAINBLOCKS_DIR}/src/core/blocks/network.cpp
  ${CHAINBLOCKS_DIR}/src/core/blocks/struct.cpp
  ${CHAINBLOCKS_DIR}/src/core/blocks/time.cpp
  ${CHAINBLOCKS_DIR}/src/core/blocks/os.cpp
  ${CHAINBLOCKS_DIR}/src/core/blocks/regex.cpp
  ${CHAINBLOCKS_DIR}/src/core/blocks/process.cpp
  ${CHAINBLOCKS_DIR}/src/core/blocks/channels.cpp
  ${CHAINBLOCKS_DIR}/src/core/edn/eval.cpp
  ${CB_EXTRAS}
  )

add_library(
  cb_static STATIC
  ${RUNTIME_SRC}
  )
set_property(TARGET cb_static PROPERTY COMPILE_FLAGS "-DCB_DLL_EXPORT")
add_dependencies(cb_static format)

set(CB_RUNTIME_LIBS cb_static)

# also shared lib target
# can't use OBJECT sadly due to xcode generator not liking it
add_library(
  cb_shared SHARED
  ${RUNTIME_SRC}
  )
set_property(TARGET cb_shared PROPERTY COMPILE_FLAGS "-DCB_DLL_EXPORT")
add_dependencies(cb_shared format)
target_link_libraries(cb_shared ${BOOST_LIBS} ${EXTRA_LIBS} ${CBLP_PLATFORM_LIBS} ${CHAINBLOCKS_EXTRA_LINKS})
###
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
add_dependencies(cbl cb_static)
include_directories(${CMAKE_CURRENT_BINARY_DIR})
set_property(TARGET cbl PROPERTY COMPILE_FLAGS "-DCB_DLL_EXPORT")
target_link_libraries(cbl -lreadline -lncurses ${CB_RUNTIME_LIBS} ${BOOST_LIBS} ${EXTRA_LIBS} ${CBLP_PLATFORM_LIBS} ${CHAINBLOCKS_EXTRA_LINKS})
###

### our experimental edn eval
add_executable(
  cbedn
  "${CHAINBLOCKS_DIR}/src/core/edn/main.cpp"
  )
add_dependencies(cbedn cb_static)
include_directories(${CMAKE_CURRENT_BINARY_DIR})
set_property(TARGET cbedn PROPERTY COMPILE_FLAGS "-DCB_DLL_EXPORT")
target_link_libraries(cbedn ${CB_RUNTIME_LIBS} ${BOOST_LIBS} ${EXTRA_LIBS} ${CBLP_PLATFORM_LIBS} ${CHAINBLOCKS_EXTRA_LINKS})
###

