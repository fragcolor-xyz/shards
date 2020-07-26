# remember to use CMAKE_EXPORT_COMPILE_COMMAND=1 with Ninja, so clang tools work out of the box
# you need to copy the .json file in the root directory
# also CMAKE_BUILD_TYPE=Debug or so

project("chainblocks")
cmake_minimum_required(VERSION 3.14)
include(ExternalProject)

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
  "-DCB_USE_TSAN -fsanitize=thread -g -O1"
  CACHE STRING "Flags used by the C++ compiler during ThreadSanitizer builds."
  FORCE)

# AddressSanitize
set(CMAKE_C_FLAGS_ASAN
  "-fsanitize=address -fno-optimize-sibling-calls -fsanitize-address-use-after-scope -fno-omit-frame-pointer -g -O1"
  CACHE STRING "Flags used by the C compiler during AddressSanitizer builds."
  FORCE)
set(CMAKE_CXX_FLAGS_ASAN
  "-DBOOST_USE_ASAN -fsanitize=address -fno-optimize-sibling-calls -fsanitize-address-use-after-scope -fno-omit-frame-pointer -g -O1"
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
  # like like: -Wl,--whole-archive -lhttp -Wl,--no-whole-archive
  # so to trigger global init and such, of course this is not ideal when deplying (size bloat!)
  set(CHAINBLOCKS_EXTRA_LINKS "")
endif()

add_compile_definitions(
  ELPP_DEFAULT_LOG_FILE="chainblocks.log"
  BOOST_INTERPROCESS_BOOTSTAMP_IS_LASTBOOTUPTIME=1)
add_compile_options(-Wall)

if(NOT EMSCRIPTEN)
  add_compile_definitions(ELPP_THREAD_SAFE)
else()
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s INLINING_LIMIT=1 -s DISABLE_EXCEPTION_CATCHING=0 -s ALLOW_MEMORY_GROWTH=1 -s ASYNCIFY=1 -s ASYNCIFY_STACK_SIZE=1073741824 -s EXIT_RUNTIME=1")
  if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s ASSERTIONS=1")
  endif()
endif()

if((WIN32 AND CMAKE_BUILD_TYPE STREQUAL "Debug") OR FORCE_USE_LLD)
  add_link_options(-fuse-ld=lld)
endif()

if(PROFILE_GPROF)
  add_compile_options(-pg -DNO_FORCE_INLINE)
  add_link_options(-pg)
endif()

if(PROFILE_COVERAGE)
  add_compile_options(--coverage)
  add_link_options(--coverage)
endif()

### general
if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
  if(WIN32)
    add_link_options(-static)
  endif()
  # add_compile_options(-flto)
  # add_link_options(-flto)
  add_compile_definitions(ELPP_DISABLE_TRACE_LOGS)
  add_compile_definitions(ELPP_DISABLE_DEBUG_LOGS)
else()
  # add_compile_definitions(ELPP_DISABLE_TRACE_LOGS)
  if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang" AND NOT EMSCRIPTEN)
    add_link_options(-fsanitize=undefined)
  endif()
endif()

add_compile_options(-ffast-math -fno-finite-math-only -funroll-loops -Wno-multichar)

if(X86 AND NOT EMSCRIPTEN)
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

if(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
  # custom allocator
  if(NOT EMSCRIPTEN)
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
endif()
###

### libs
if(WIN32)
  set(BOOST_LIBS -lboost_context-mt -lboost_filesystem-mt -lssl -lcrypto)
  set(EXTRA_LIBS -lws2_32)
elseif(APPLE)
  if(NOT IOS)
    set(BOOST_LIBS -lboost_context-mt -lssl -lcrypto)
  endif()
  include_directories(
    /usr/local/include
    /usr/local/opt/openssl/include
    )
  link_directories(
    /usr/local/lib
    /usr/local/opt/openssl/lib
    )
elseif(EMSCRIPTEN)
  include_directories(${CHAINBLOCKS_DIR}/external/boost_1_73_0)
elseif(UNIX)
  set(BOOST_LIBS -lboost_context -lssl -lcrypto)
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
  ${CHAINBLOCKS_DIR}/include/utility.hpp
  ${CHAINBLOCKS_DIR}/include/asiotools.hpp
  ${CHAINBLOCKS_DIR}/include/chainblocks.hpp
  ${CHAINBLOCKS_DIR}/include/chainblocks.h
  ${CHAINBLOCKS_DIR}/include/ops.hpp
  ${CHAINBLOCKS_DIR}/src/core/runtime.cpp
  ${CHAINBLOCKS_DIR}/src/core/runtime.hpp
  ${CHAINBLOCKS_DIR}/src/core/foundation.hpp
  ${CHAINBLOCKS_DIR}/src/core/ops_internal.hpp
  ${CHAINBLOCKS_DIR}/src/core/blocks/process.cpp
  ${CHAINBLOCKS_DIR}/src/core/blocks_macros.hpp
  ${CHAINBLOCKS_DIR}/src/core/coro.cpp
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
  ${CHAINBLOCKS_DIR}/src/core/blocks/python.cpp
  ${CHAINBLOCKS_DIR}/src/core/blocks/lmdb.cpp
  ${CHAINBLOCKS_DIR}/src/core/blocks/genetic.cpp
  ${CHAINBLOCKS_DIR}/src/core/blocks/random.cpp
  ${CHAINBLOCKS_DIR}/src/core/blocks/imaging.cpp
  ${CHAINBLOCKS_DIR}/src/core/blocks/http.cpp
  ${CHAINBLOCKS_DIR}/src/extra/bgfx.cpp
  ${CHAINBLOCKS_DIR}/src/extra/bgfx.hpp
  ${CHAINBLOCKS_DIR}/src/extra/imgui.cpp
  ${CHAINBLOCKS_DIR}/src/extra/imgui.hpp
  ${CHAINBLOCKS_DIR}/src/extra/runtime.cpp
  ${CHAINBLOCKS_DIR}/src/extra/desktop.hpp
  ${CHAINBLOCKS_DIR}/src/extra/desktop.win.cpp
  ${CHAINBLOCKS_DIR}/src/extra/desktop.capture.win.hpp
  ${CHAINBLOCKS_DIR}/src/mal/CBCore.cpp
  ${CHAINBLOCKS_DIR}/src/core/blocks/serialization.cpp
  ${CHAINBLOCKS_DIR}/src/core/blocks/time.cpp
  ${CHAINBLOCKS_DIR}/src/core/blocks/os.cpp
  ${CHAINBLOCKS_DIR}/src/core/blocks/regex.cpp
  ${CHAINBLOCKS_DIR}/src/core/edn/print.hpp
  ${CHAINBLOCKS_DIR}/src/core/edn/read.hpp
  ${CHAINBLOCKS_DIR}/src/core/edn/eval.hpp
  ${CHAINBLOCKS_DIR}/src/core/edn/eval.cpp
  ${CHAINBLOCKS_DIR}/src/extra/snappy.cpp
  ${CHAINBLOCKS_DIR}/src/core/blocks/ws.cpp
  ${CHAINBLOCKS_DIR}/src/core/blocks/bigint.cpp
  )

#### Header paths for tidy
set(
  MY_PROJECT_HEADER_PATHS
  ${MY_PROJECT_HEADER_PATHS}
  -I${CHAINBLOCKS_DIR}/include
  -I${CHAINBLOCKS_DIR}/src/core
  -I${CHAINBLOCKS_DIR}/deps/replxx/include
  -I${CHAINBLOCKS_DIR}/deps/easyloggingpp/src
  -I${CHAINBLOCKS_DIR}/deps/SPSCQueue/include
  -I${CHAINBLOCKS_DIR}/deps/stb
  -I${CHAINBLOCKS_DIR}/deps/kcp
  -I${CHAINBLOCKS_DIR}/deps/xxHash
  -I${CHAINBLOCKS_DIR}/deps/json/include
  -I${CHAINBLOCKS_DIR}/deps/magic_enum/include
  -I${CHAINBLOCKS_DIR}/deps/nameof/include
  -I${CHAINBLOCKS_DIR}/deps/lmdb/libraries/liblmdb
  -I${CHAINBLOCKS_DIR}/deps/cpp-taskflow
  -I${CHAINBLOCKS_DIR}/deps/pdqsort
  -I${CHAINBLOCKS_DIR}/deps/filesystem/include
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
foreach(_file ${MY_PROJECT_SOURCE_FILES})
  if(CLANG_FORMAT_EXE)
    add_custom_command(
      OUTPUT ${_file}.formatted
      COMMAND ${CLANG_FORMAT_EXE} -i -style=LLVM ${_file}
      )
    list(APPEND formatfied ${_file}.formatted)
  endif()
endforeach()
add_custom_target(format DEPENDS ${formatfied})
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
      OUTPUT ${_file}.tidyfied
      COMMAND ${CLANG_TIDY_EXE} -checks=-*,clang-analyzer-*,performance-*,bugprone-* -fix ${_file} -- -std=c++17 -DDEBUG ${MY_PROJECT_HEADER_PATHS}
      )
    list(APPEND tidyfied ${_file}.tidyfied)
  endif()
endforeach()
add_custom_target(tidy DEPENDS ${tidyfied})
###

if(UNIX)
  add_compile_options(-fvisibility=hidden)
endif()

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
  ${CHAINBLOCKS_DIR}/deps/replxx/include
  ${CHAINBLOCKS_DIR}/deps/easyloggingpp/src
  ${CHAINBLOCKS_DIR}/deps/SPSCQueue/include
  ${CHAINBLOCKS_DIR}/deps/stb
  ${CHAINBLOCKS_DIR}/deps/xxHash
  ${CHAINBLOCKS_DIR}/deps/json/include
  ${CHAINBLOCKS_DIR}/deps/magic_enum/include
  ${CHAINBLOCKS_DIR}/deps/nameof/include
  ${CHAINBLOCKS_DIR}/deps/kcp
  ${CHAINBLOCKS_DIR}/deps/lmdb/libraries/liblmdb
  ${CHAINBLOCKS_DIR}/deps/cpp-taskflow
  ${CHAINBLOCKS_DIR}/include
  ${CHAINBLOCKS_DIR}/src/core
  ${CHAINBLOCKS_DIR}/deps/pdqsort
  ${CHAINBLOCKS_DIR}/deps/filesystem/include
  )

if(NOT BUILD_CORE_ONLY)
  add_compile_definitions(
    CB_WITH_EXTRAS 
    BIMG_DECODE_ENABLE=0
    SDL_RENDER_DISABLED=1
    )

  include_directories(
    ${CHAINBLOCKS_DIR}/deps/bx/include
    ${CHAINBLOCKS_DIR}/deps/bx/3rdparty
    ${CHAINBLOCKS_DIR}/deps/bgfx/include
    ${CHAINBLOCKS_DIR}/deps/bgfx/3rdparty
    ${CHAINBLOCKS_DIR}/deps/imgui_club/imgui_memory_editor
    ${CHAINBLOCKS_DIR}/deps/bgfx/examples/common/imgui
    ${CHAINBLOCKS_DIR}/deps/bimg/include
    ${CHAINBLOCKS_DIR}/deps/bgfx/3rdparty/dear-imgui
    ${CHAINBLOCKS_DIR}/deps/implot
    ${CHAINBLOCKS_DIR}/deps/bimg/3rdparty/astc-codec/include
    )

  set(SDL-source-patterns
    ${CHAINBLOCKS_DIR}/deps/SDL/src/*.c
    ${CHAINBLOCKS_DIR}/deps/SDL/src/audio/*.c
    ${CHAINBLOCKS_DIR}/deps/SDL/src/audio/dummy/*.c
    ${CHAINBLOCKS_DIR}/deps/SDL/src/cpuinfo/*.c
    ${CHAINBLOCKS_DIR}/deps/SDL/src/events/*.c
    ${CHAINBLOCKS_DIR}/deps/SDL/src/file/*.c
    ${CHAINBLOCKS_DIR}/deps/SDL/src/haptic/*.c
    ${CHAINBLOCKS_DIR}/deps/SDL/src/haptic/dummy/*.c
    ${CHAINBLOCKS_DIR}/deps/SDL/src/joystick/*.c
    ${CHAINBLOCKS_DIR}/deps/SDL/src/joystick/dummy/*.c
    ${CHAINBLOCKS_DIR}/deps/SDL/src/loadso/dummy/*.c
    ${CHAINBLOCKS_DIR}/deps/SDL/src/power/*.c
    ${CHAINBLOCKS_DIR}/deps/SDL/src/filesystem/dummy/*.c
    ${CHAINBLOCKS_DIR}/deps/SDL/src/render/*.c
    ${CHAINBLOCKS_DIR}/deps/SDL/src/render/software/*.c
    ${CHAINBLOCKS_DIR}/deps/SDL/src/sensor/*.c
    ${CHAINBLOCKS_DIR}/deps/SDL/src/sensor/dummy/*.c
    ${CHAINBLOCKS_DIR}/deps/SDL/src/stdlib/*.c
    ${CHAINBLOCKS_DIR}/deps/SDL/src/thread/*.c
    ${CHAINBLOCKS_DIR}/deps/SDL/src/timer/*.c
    ${CHAINBLOCKS_DIR}/deps/SDL/src/timer/dummy/*.c
    ${CHAINBLOCKS_DIR}/deps/SDL/src/video/*.c
    ${CHAINBLOCKS_DIR}/deps/SDL/src/video/dummy/*.c
  )

  file(GLOB SDL-sources ${SDL-source-patterns})

  if(WIN32)
    set(libsdlfile ${CMAKE_CURRENT_BINARY_DIR}/sdl_a/lib/libSDL2-static.a)
  else()
    set(libsdlfile ${CMAKE_CURRENT_BINARY_DIR}/sdl_a/lib/libSDL2.a)
  endif()

  ExternalProject_Add(sdl_a
    GIT_REPOSITORY    https://github.com/chainblocks/SDL.git
    GIT_TAG           de56c2764424f884eeb83d07c19d51ff284101a6
    PREFIX ${CMAKE_CURRENT_BINARY_DIR}/sdl_a
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX:PATH=${CMAKE_CURRENT_BINARY_DIR}/sdl_a
    BUILD_BYPRODUCTS ${libsdlfile}
  )

  add_library(libsdl STATIC IMPORTED GLOBAL)
  set_target_properties(libsdl PROPERTIES IMPORTED_LOCATION ${libsdlfile})
  add_dependencies(libsdl sdl_a)

  if(WIN32)
    add_compile_definitions(BGFX_CONFIG_RENDERER_DIRECT3D11)

    include_directories(
      ${CHAINBLOCKS_DIR}/deps/bx/include/compat/mingw
      ${CMAKE_CURRENT_BINARY_DIR}/sdl_a/include/SDL2
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
        ${CBLP_PLATFORM_LIBS}
	      )
    elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
      # 32 bits
      set(CBLP_PLATFORM_LIBS
        ${CBLP_PLATFORM_LIBS}
	      )
    endif()
  elseif(APPLE)  
    add_compile_definitions(BGFX_CONFIG_RENDERER_METAL SDL_VIDEO_OPENGL_EGL=0)

    if(IOS)
      file(GLOB SDL-extra-sources 
        ${CHAINBLOCKS_DIR}/deps/SDL/src/video/uikit/*.m
        ${CHAINBLOCKS_DIR}/deps/SDL/src/power/uikit/*.m
        ${CHAINBLOCKS_DIR}/deps/SDL/src/audio/coreaudio/*.m
        ${CHAINBLOCKS_DIR}/deps/SDL/src/thread/pthread/*.c
        ${CHAINBLOCKS_DIR}/deps/SDL/src/sensor/coremotion/*.m
        ${CHAINBLOCKS_DIR}/deps/SDL/src/joystick/iphoneos/*.m
        )
      add_compile_definitions(SDL_VIDEO_DRIVER_UIKIT=1 CB_NO_HTTP_BLOCKS=1 SDL_MAIN_HANDLED=1)
    else()
      file(GLOB SDL-extra-sources 
        ${CHAINBLOCKS_DIR}/deps/SDL/src/video/cocoa/*.c
        ${CHAINBLOCKS_DIR}/deps/SDL/src/video/cocoa/*.m
        ${CHAINBLOCKS_DIR}/deps/SDL/src/audio/coreaudio/*.m
        ${CHAINBLOCKS_DIR}/deps/SDL/src/thread/pthread/*.c
        )
      add_compile_definitions(SDL_VIDEO_DRIVER_COCOA=1)
    endif()

    include_directories(
      ${CHAINBLOCKS_DIR}/deps/bx/include/compat/osx
      ${CHAINBLOCKS_DIR}/deps/SDL/include
      ${CHAINBLOCKS_DIR}/deps/SDL/src/hidapi/hidapi
      )

    set(PLATFORM_SOURCES
      ${SDL-sources}
      ${SDL-extra-sources}
      ${CHAINBLOCKS_DIR}/deps/SDL/src/atomic/SDL_atomic.c
      ${CHAINBLOCKS_DIR}/deps/SDL/src/atomic/SDL_spinlock.c
      ${CHAINBLOCKS_DIR}/deps/SDL/src/dynapi/SDL_dynapi.c
      ${CHAINBLOCKS_DIR}/deps/SDL/src/timer/unix/SDL_systimer.c
      ${CHAINBLOCKS_DIR}/deps/SDL/src/audio/disk/SDL_diskaudio.c
      ${CHAINBLOCKS_DIR}/deps/SDL/src/hidapi/mac/hid.c
      ${CHAINBLOCKS_DIR}/deps/SDL/src/joystick/hidapi/SDL_hidapijoystick.c
      ${CHAINBLOCKS_DIR}/deps/SDL/src/joystick/hidapi/SDL_hidapi_gamecube.c
      ${CHAINBLOCKS_DIR}/deps/SDL/src/joystick/hidapi/SDL_hidapi_ps4.c
      ${CHAINBLOCKS_DIR}/deps/SDL/src/joystick/hidapi/SDL_hidapi_rumble.c
      ${CHAINBLOCKS_DIR}/deps/SDL/src/joystick/hidapi/SDL_hidapi_switch.c
      ${CHAINBLOCKS_DIR}/deps/SDL/src/joystick/hidapi/SDL_hidapi_xbox360.c
      ${CHAINBLOCKS_DIR}/deps/SDL/src/joystick/hidapi/SDL_hidapi_xbox360w.c
      ${CHAINBLOCKS_DIR}/deps/SDL/src/joystick/hidapi/SDL_hidapi_xboxone.c
      ${CHAINBLOCKS_DIR}/deps/SDL/src/joystick/virtual/SDL_virtualjoystick.c
      ${CHAINBLOCKS_DIR}/deps/SDL/src/joystick/darwin/SDL_sysjoystick.c
      ${CHAINBLOCKS_DIR}/deps/SDL/src/haptic/darwin/SDL_syshaptic.c
      ${CHAINBLOCKS_DIR}/deps/SDL/src/video/yuv2rgb/yuv_rgb.c
      ${CHAINBLOCKS_DIR}/deps/SDL/src/power/macosx/SDL_syspower.c
      ${CHAINBLOCKS_DIR}/deps/SDL/src/filesystem/cocoa/SDL_sysfilesystem.m
      ${CHAINBLOCKS_DIR}/deps/SDL/src/loadso/dlopen/SDL_sysloadso.c
      ${CHAINBLOCKS_DIR}/deps/SDL/src/locale/SDL_locale.c
      ${CHAINBLOCKS_DIR}/deps/SDL/src/locale/macosx/SDL_syslocale.m
      ${CHAINBLOCKS_DIR}/deps/SDL/src/file/cocoa/SDL_rwopsbundlesupport.m

      ${CHAINBLOCKS_DIR}/deps/bgfx/src/amalgamated.mm
      ${CHAINBLOCKS_DIR}/src/extra/bgfx.mm
      )

    if(IOS)
      set(PLATFORM_SOURCES 
        ${PLATFORM_SOURCES}
        ${CHAINBLOCKS_DIR}/deps/boost-context/src/posix/stack_traits.cpp
        ${CHAINBLOCKS_DIR}/deps/boost-context/src/asm/jump_x86_64_sysv_macho_gas.S
        ${CHAINBLOCKS_DIR}/deps/boost-context/src/asm/make_x86_64_sysv_macho_gas.S
        ${CHAINBLOCKS_DIR}/deps/boost-context/src/asm/ontop_x86_64_sysv_macho_gas.S
      )
    endif()

    set(CBLP_PLATFORM_LIBS
      "-framework Foundation"
      "-framework CoreAudio"
      "-framework AudioToolbox"
      "-framework CoreVideo"
      "-framework IOKit"
      "-framework QuartzCore"
      "-framework Metal"
      )
    
    if(IOS)
      set(CBLP_PLATFORM_LIBS
          ${CBLP_PLATFORM_LIBS}
          "-framework AVFoundation"
          "-framework GameController"
          "-framework CoreMotion" 
          "-framework CoreGraphics"     
          "-framework UIKit")
    else()
       set(CBLP_PLATFORM_LIBS
          ${CBLP_PLATFORM_LIBS}
          "-framework Cocoa"
          "-framework Carbon"
          "-framework ForceFeedback")
    endif()
  elseif(LINUX)
    add_compile_definitions(BGFX_CONFIG_RENDERER_OPENGL)

    include_directories(
      ${CHAINBLOCKS_DIR}/deps/bgfx/3rdparty/khronos
      ${CMAKE_CURRENT_BINARY_DIR}/sdl_a/include/SDL2
      )
      
    set(PLATFORM_SOURCES
      ${CHAINBLOCKS_DIR}/deps/bgfx/src/amalgamated.cpp
      )

    set(CBLP_PLATFORM_LIBS
      -lX11
      -lGL
      )
  endif()

  if(NOT CB_NO_SNAPPY_BLOCKS AND NOT IOS)
    ExternalProject_Add(snappy_a
      GIT_REPOSITORY    https://github.com/chainblocks/snappy.git
      GIT_TAG           f5acee902c4d2110f671455460172cb6d3bf5b73
      PREFIX ${CMAKE_CURRENT_BINARY_DIR}/snappy_a
      CMAKE_ARGS -DCMAKE_INSTALL_PREFIX:PATH=${CMAKE_CURRENT_BINARY_DIR}/snappy_a
      BUILD_BYPRODUCTS ${CMAKE_CURRENT_BINARY_DIR}/snappy_a/lib/libsnappy.a
      )

    include_directories(${CMAKE_CURRENT_BINARY_DIR}/snappy_a/include)

    add_library(libsnappy STATIC IMPORTED GLOBAL)
    add_dependencies(libsnappy snappy_a)
    set_target_properties(libsnappy PROPERTIES IMPORTED_LOCATION ${CMAKE_CURRENT_BINARY_DIR}/snappy_a/lib/libsnappy.a)
    
    set(CB_EXTRAS ${CB_EXTRAS} ${CHAINBLOCKS_DIR}/src/extra/snappy.cpp)
  else()
    add_compile_definitions(CB_NO_SNAPPY_BLOCKS=1)
  endif()

  set(CB_EXTRAS
    ${CB_EXTRAS}
    ${CHAINBLOCKS_DIR}/src/extra/runtime.cpp
    ${CHAINBLOCKS_DIR}/src/extra/bgfx.cpp
    ${CHAINBLOCKS_DIR}/src/extra/imgui.cpp
    ${CHAINBLOCKS_DIR}/deps/bimg/src/image.cpp
    ${CHAINBLOCKS_DIR}/deps/bimg/src/image_gnf.cpp
    ${CHAINBLOCKS_DIR}/deps/bx/src/amalgamated.cpp
    ${CHAINBLOCKS_DIR}/deps/bgfx/examples/common/imgui/imgui.cpp
    ${CHAINBLOCKS_DIR}/deps/bgfx/3rdparty/dear-imgui/imgui.cpp
    ${CHAINBLOCKS_DIR}/deps/bgfx/3rdparty/dear-imgui/imgui_draw.cpp
    ${CHAINBLOCKS_DIR}/deps/bgfx/3rdparty/dear-imgui/imgui_widgets.cpp
    ${CHAINBLOCKS_DIR}/deps/implot/implot.cpp
    # ${CHAINBLOCKS_DIR}/deps/implot/implot_demo.cpp
    ${PLATFORM_SOURCES}
    )
endif()

add_compile_definitions(REPLXX_STATIC)
set(replxx-source-patterns ${CHAINBLOCKS_DIR}/deps/replxx/src/*.cpp ${CHAINBLOCKS_DIR}/deps/replxx/src/*.cxx)
file(GLOB replxx-sources ${replxx-source-patterns})

set(RUNTIME_SRC
  ${CHAINBLOCKS_DIR}/deps/easyloggingpp/src/easylogging++.cc
  ${replxx-sources}
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
  ${CHAINBLOCKS_DIR}/src/core/blocks/json.cpp
  ${CHAINBLOCKS_DIR}/src/core/blocks/struct.cpp
  ${CHAINBLOCKS_DIR}/src/core/blocks/time.cpp
  ${CHAINBLOCKS_DIR}/src/core/blocks/regex.cpp
  ${CHAINBLOCKS_DIR}/src/core/blocks/channels.cpp
  ${CHAINBLOCKS_DIR}/src/core/blocks/random.cpp
  ${CHAINBLOCKS_DIR}/src/core/blocks/imaging.cpp
  ${CHAINBLOCKS_DIR}/src/core/blocks/ws.cpp
  ${CHAINBLOCKS_DIR}/src/core/blocks/bigint.cpp
  ${CB_EXTRAS}
  )

if(NOT CB_NO_BIGINT_BLOCKS)
  set(
    RUNTIME_SRC 
    ${RUNTIME_SRC}
    ${CHAINBLOCKS_DIR}/src/core/blocks/bigint.cpp)
else()
  add_compile_definitions(CB_NO_BIGINT_BLOCKS=1)
endif()

if(NOT EMSCRIPTEN)
  set(
    RUNTIME_SRC
    ${RUNTIME_SRC}
    ${CHAINBLOCKS_DIR}/deps/lmdb/libraries/liblmdb/mdb.c
    ${CHAINBLOCKS_DIR}/deps/lmdb/libraries/liblmdb/midl.c
    ${CHAINBLOCKS_DIR}/src/core/blocks/lmdb.cpp
    ${CHAINBLOCKS_DIR}/src/core/blocks/fs.cpp
    ${CHAINBLOCKS_DIR}/src/core/blocks/process.cpp
    ${CHAINBLOCKS_DIR}/src/core/blocks/os.cpp
    ${CHAINBLOCKS_DIR}/src/core/blocks/network.cpp
    ${CHAINBLOCKS_DIR}/src/core/blocks/genetic.cpp
    ${CHAINBLOCKS_DIR}/src/core/blocks/python.cpp
    ${CHAINBLOCKS_DIR}/src/core/blocks/http.cpp
    ${CHAINBLOCKS_DIR}/src/core/edn/eval.cpp
    )
else()
  set(
    RUNTIME_SRC
    ${RUNTIME_SRC}
    ${CHAINBLOCKS_DIR}/src/core/coro.cpp
    )
endif()

add_library(
  cb_static STATIC
  ${RUNTIME_SRC}
  )
set_property(TARGET cb_static PROPERTY COMPILE_FLAGS "-DCB_DLL_EXPORT")

set(CB_RUNTIME_LIBS cb_static)

# also shared lib target
# can't use OBJECT sadly due to xcode generator not liking it
add_library(
  cb_shared SHARED
  ${RUNTIME_SRC}
  )
set_property(TARGET cb_shared PROPERTY COMPILE_FLAGS "-DCB_DLL_EXPORT")
target_link_libraries(cb_shared ${BOOST_LIBS} ${EXTRA_LIBS} ${CBLP_PLATFORM_LIBS} ${CHAINBLOCKS_EXTRA_LINKS})
###
###

if(NOT BUILD_CORE_ONLY)
  if(LINUX OR WIN32)
    add_dependencies(cb_static sdl_a)
    add_dependencies(cb_shared sdl_a)
    target_link_libraries(cb_shared libsdl)
    set(EXTRA_LIBS ${EXTRA_LIBS} libsdl)
  endif()

  if(NOT CB_NO_SNAPPY_BLOCKS AND NOT IOS)
    add_dependencies(cb_static snappy_a)
    target_link_libraries(cb_static libsnappy)
    add_dependencies(cb_shared snappy_a)
    target_link_libraries(cb_shared libsnappy)
  endif()
endif()

if(USE_LIBBACKTRACE)
  set(EXTRA_LIBS ${EXTRA_LIBS} ${CHAINBLOCKS_DIR}/deps/libbacktrace/build/lib/libbacktrace.a)
  add_compile_definitions(BOOST_STACKTRACE_USE_BACKTRACE)
  include_directories(${CHAINBLOCKS_DIR}/deps/libbacktrace/build/include)
endif()

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
target_link_libraries(cbl ${CB_RUNTIME_LIBS} ${BOOST_LIBS} ${EXTRA_LIBS} ${CBLP_PLATFORM_LIBS} ${CHAINBLOCKS_EXTRA_LINKS})
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
