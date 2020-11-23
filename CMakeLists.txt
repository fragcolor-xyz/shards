# This needs a huge refactor at some point...

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

if(CODE_COVERAGE)
  if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    add_compile_options(-fprofile-arcs -ftest-coverage)
    add_link_options(-fprofile-arcs -ftest-coverage)
  endif()
endif()

if (NOT DEFINED CHAINBLOCKS_DIR)
  set(CHAINBLOCKS_DIR "${CMAKE_CURRENT_LIST_DIR}")
endif()

if (NOT DEFINED CHAINBLOCKS_EXTRA_LINKS)
  set(CHAINBLOCKS_EXTRA_LINKS "")
endif()

add_compile_definitions(
  ELPP_DEFAULT_LOG_FILE="chainblocks.log"
  BOOST_INTERPROCESS_BOOTSTAMP_IS_LASTBOOTUPTIME=1)
add_compile_options(-Wall)

if(NOT EMSCRIPTEN)
  add_compile_definitions(ELPP_THREAD_SAFE)
else()
  # build with cmake -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_TOOLCHAIN_FILE=$EMSDK/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake ..
  # MinSizeRel is better for release builds
  # test with node cbl.js -e '(do (def Root (Node)) (schedule Root (Chain "x" (Msg "Hello"))) (run Root 0.1))'
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s INLINING_LIMIT=1 -s DISABLE_EXCEPTION_CATCHING=0 -s ALLOW_MEMORY_GROWTH=1 -s ASYNCIFY=1 -s EXIT_RUNTIME=1 -s MAX_WEBGL_VERSION=2 -s USE_SDL=2")
  if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s ASSERTIONS=1")
    add_compile_options(-g2 -Oz)
  endif()
  add_compile_definitions(NO_FORCE_INLINE)
  ## if we wanted thread support...
  if(EMSCRIPTEN_PTHREADS)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s USE_PTHREADS=1 -s PTHREAD_POOL_SIZE=2")
    add_compile_options(-pthread -Wno-pthreads-mem-growth)
    add_link_options(-pthread)
  endif()
endif()

if((WIN32 AND CMAKE_BUILD_TYPE STREQUAL "Debug") OR FORCE_USE_LLD)
  add_link_options(-fuse-ld=lld)
  SET(CMAKE_AR llvm-ar)
  SET(CMAKE_RANLIB llvm-ranlib)
endif()

if(PROFILE_GPROF)
  add_compile_options(-pg -DNO_FORCE_INLINE)
  add_link_options(-pg)
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
    add_compile_options(-fsanitize=undefined)
    add_link_options(-fsanitize=undefined)
  endif()
endif()

add_compile_options(-ffast-math -fno-finite-math-only -funroll-loops -Wno-multichar)

if(X86 AND NOT EMSCRIPTEN)
  if(FORCE_CORE2)
    # Not as good as AVX but CBVarPayload being m128 and such rocks it!
    add_compile_options(-march=core2)
  elseif(FORCE_I686)
    # no vectors at all
    add_compile_options(-march=i686)
  elseif(FORCE_NATIVE)
    # the best but low compatibility
    add_compile_options(-march=native)
  else()
    # CBVars are m256 basically, so this is extremely good! (AVX!)
    add_compile_options(-march=sandybridge)
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
  set(BOOST_LIBS -lboost_context-mt -lboost_filesystem-mt -lssl -lcrypto -lws2_32 -lmswsock)
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
  include_directories(${CHAINBLOCKS_DIR}/external/boost_1_74_0)
elseif(UNIX)
  set(BOOST_LIBS -lboost_context -lssl -lcrypto -pthread -ldl -lrt)
endif()

if(APPLE)
  add_compile_definitions(BOOST_STACKTRACE_GNU_SOURCE_NOT_REQUIRED)
  add_compile_options(-Wextra -Wno-unused-parameter -Wno-missing-field-initializers)
  include_directories(/usr/local/include/boost)
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
  ${CHAINBLOCKS_DIR}/include/common_types.hpp
  ${CHAINBLOCKS_DIR}/include/ops.hpp
  ${CHAINBLOCKS_DIR}/src/core/runtime.cpp
  ${CHAINBLOCKS_DIR}/src/core/ops_internal.cpp
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
  ${CHAINBLOCKS_DIR}/src/extra/brotli.cpp
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

if(EMSCRIPTEN)
  set(EXTRA_CMAKE_ARGS -DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE})
elseif(IOS)
  add_compile_definitions(CB_NO_HTTP_BLOCKS=1)
  set(EXTRA_CMAKE_ARGS -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} -DCMAKE_SYSTEM_NAME=${CMAKE_SYSTEM_NAME})
  if(X86_IOS_SIMULATOR)
    set(EXTRA_CMAKE_BUILD_ARGS -- -sdk iphonesimulator -arch x86_64)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
      set(EXTRA_LIBS_DIR "Debug-iphonesimulator")
    else()
      set(EXTRA_LIBS_DIR "Release-iphonesimulator")
    endif()
  else()
    set(EXTRA_CMAKE_BUILD_ARGS -- -sdk iphoneos -arch arm64)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
      set(EXTRA_LIBS_DIR "Debug-iphoneos")
    else()
      set(EXTRA_LIBS_DIR "Release-iphoneos")
    endif()
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
    BGFX_SHADERC_NO_MAIN
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

    # shader compiler related
    ${CHAINBLOCKS_DIR}/deps/bgfx/3rdparty/fcpp
    ${CHAINBLOCKS_DIR}/deps/bgfx/tools/shaderc
    )

  if(WIN32)
    set(libsdlfile ${CMAKE_CURRENT_BINARY_DIR}/sdl_a/lib/libSDL2-static.a)
  elseif(X86_IOS_SIMULATOR)
    set(libsdlfile ${CMAKE_CURRENT_BINARY_DIR}/sdl_a/src/sdl_a/Xcode/SDL/build/Release-iphonesimulator/libSDL2.a)
    add_link_options(-F${CMAKE_CURRENT_BINARY_DIR}/sdl_a/src/sdl_a/Xcode/SDL/build/Release-iphonesimulator)
  elseif(IOS)
    set(libsdlfile ${CMAKE_CURRENT_BINARY_DIR}/sdl_a/src/sdl_a/Xcode/SDL/build/Release-iphoneos/libSDL2.a)
    add_link_options(-F${CMAKE_CURRENT_BINARY_DIR}/sdl_a/src/sdl_a/Xcode/SDL/build/Release-iphoneos)
  else()
    set(libsdlfile ${CMAKE_CURRENT_BINARY_DIR}/sdl_a/lib/libSDL2.a)
  endif()
  
  if(X86_IOS_SIMULATOR)
    ExternalProject_Add(sdl_a
      URL ${CHAINBLOCKS_DIR}/deps/SDL
      PREFIX ${CMAKE_CURRENT_BINARY_DIR}/sdl_a
      CONFIGURE_COMMAND ""
      BUILD_IN_SOURCE true
      BUILD_COMMAND cd Xcode/SDL && xcodebuild -configuration Release -target "Static Library-iOS" -target hidapi-iOS -sdk iphonesimulator -arch x86_64
      INSTALL_COMMAND ""
      BUILD_BYPRODUCTS ${libsdlfile}
    )
  elseif(IOS)
    ExternalProject_Add(sdl_a
      URL ${CHAINBLOCKS_DIR}/deps/SDL
      PREFIX ${CMAKE_CURRENT_BINARY_DIR}/sdl_a
      CONFIGURE_COMMAND ""
      BUILD_IN_SOURCE true
      BUILD_COMMAND cd Xcode/SDL && xcodebuild -configuration Release -target "Static Library-iOS" -target hidapi-iOS -sdk iphoneos -arch arm64
      INSTALL_COMMAND ""
      BUILD_BYPRODUCTS ${libsdlfile}
  )
  else()
    ExternalProject_Add(sdl_a
      URL ${CHAINBLOCKS_DIR}/deps/SDL
      PREFIX ${CMAKE_CURRENT_BINARY_DIR}/sdl_a
      CMAKE_ARGS -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX:PATH=${CMAKE_CURRENT_BINARY_DIR}/sdl_a ${EXTRA_CMAKE_ARGS}
      BUILD_BYPRODUCTS ${libsdlfile}
    )
  endif()

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
      ${CHAINBLOCKS_DIR}/deps/bgfx/tools/shaderc/shaderc_hlsl.cpp
      ${CHAINBLOCKS_DIR}/src/extra/desktop.win.cpp
      )

    set(CB_PLATFORM_LIBS
      -lntdll -lOle32 -lImm32 -lWinmm -lGdi32 -lVersion -lOleAut32 -lSetupAPI -lPsapi -lD3D11 -lDXGI -lUserenv
      )

    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
      # 64 bits
      set(CB_PLATFORM_LIBS
        ${CB_PLATFORM_LIBS}
	      )
    elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
      # 32 bits
      set(CB_PLATFORM_LIBS
        ${CB_PLATFORM_LIBS}
	      )
    endif()
  elseif(APPLE)  
    add_compile_definitions(BGFX_CONFIG_RENDERER_METAL SDL_VIDEO_OPENGL_EGL=0)

    include_directories(
      ${CHAINBLOCKS_DIR}/deps/bx/include/compat/osx
      ${CHAINBLOCKS_DIR}/deps/SDL/include
      ${CHAINBLOCKS_DIR}/deps/SDL/src/hidapi/hidapi
      )

    set(PLATFORM_SOURCES
      ${CHAINBLOCKS_DIR}/deps/bgfx/src/amalgamated.mm
      )

    if(IOS)
      enable_language(ASM)
      
      if(X86 OR X86_IOS_SIMULATOR)
        set(PLATFORM_SOURCES 
          ${PLATFORM_SOURCES}
          ${CHAINBLOCKS_DIR}/deps/boost-context/src/posix/stack_traits.cpp
          ${CHAINBLOCKS_DIR}/deps/boost-context/src/asm/jump_x86_64_sysv_macho_gas.S
          ${CHAINBLOCKS_DIR}/deps/boost-context/src/asm/make_x86_64_sysv_macho_gas.S
          ${CHAINBLOCKS_DIR}/deps/boost-context/src/asm/ontop_x86_64_sysv_macho_gas.S
        )
      else()
        set(PLATFORM_SOURCES 
          ${PLATFORM_SOURCES}
          ${CHAINBLOCKS_DIR}/deps/boost-context/src/posix/stack_traits.cpp
          ${CHAINBLOCKS_DIR}/deps/boost-context/src/asm/jump_arm64_aapcs_macho_gas.S
          ${CHAINBLOCKS_DIR}/deps/boost-context/src/asm/make_arm64_aapcs_macho_gas.S
          ${CHAINBLOCKS_DIR}/deps/boost-context/src/asm/ontop_arm64_aapcs_macho_gas.S
      )
      endif()
    endif()

    set(CB_PLATFORM_LIBS
      "-framework Foundation"
      "-framework CoreAudio"
      "-framework AudioToolbox"
      "-framework CoreVideo"
      "-framework IOKit"
      "-framework QuartzCore"
      "-framework Metal"
      -liconv
      )
    
    if(IOS)
      set(CB_PLATFORM_LIBS
          ${CB_PLATFORM_LIBS}
          "-framework hidapi"
          "-framework AVFoundation"
          "-framework GameController"
          "-framework CoreMotion" 
          "-framework CoreGraphics"
          "-framework CoreHaptics"
          "-framework Security"
          "-framework UIKit")
    else()
       set(CB_PLATFORM_LIBS
          ${CB_PLATFORM_LIBS}
          "-framework Cocoa"
          "-framework Carbon"
          "-framework ForceFeedback")
    endif()
  elseif(LINUX)
    if(NOT EMSCRIPTEN)
      add_compile_definitions(BGFX_CONFIG_RENDERER_OPENGL)
    else()
      add_compile_definitions(BGFX_CONFIG_RENDERER_OPENGLES)  
    endif()

    include_directories(
      ${CHAINBLOCKS_DIR}/deps/bgfx/3rdparty/khronos
      ${CMAKE_CURRENT_BINARY_DIR}/sdl_a/include/SDL2
      )
      
    set(PLATFORM_SOURCES
      ${CHAINBLOCKS_DIR}/deps/bgfx/src/amalgamated.cpp
      ${CHAINBLOCKS_DIR}/deps/bgfx/tools/shaderc/shaderc_glsl.cpp
      )

    set(CB_PLATFORM_LIBS
      -lX11
      -lGL
      )

    ExternalProject_Add(glsl-optimizer_a
      URL     ${CHAINBLOCKS_DIR}/deps/bgfx/3rdparty/glsl-optimizer
      PREFIX ${CMAKE_CURRENT_BINARY_DIR}/glsl-optimizer_a
      CMAKE_ARGS -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX:PATH=${CMAKE_CURRENT_BINARY_DIR}/glsl-optimizer_a ${EXTRA_CMAKE_ARGS}
      BUILD_COMMAND cmake --build . --target glsl_optimizer glcpp-library mesa
      INSTALL_COMMAND ""
      BUILD_BYPRODUCTS ${CMAKE_CURRENT_BINARY_DIR}/glsl-optimizer_a/src/glsl-optimizer_a-build/libglsl_optimizer.a 
      BUILD_BYPRODUCTS ${CMAKE_CURRENT_BINARY_DIR}/glsl-optimizer_a/src/glsl-optimizer_a-build/libglcpp-library.a 
      BUILD_BYPRODUCTS ${CMAKE_CURRENT_BINARY_DIR}/glsl-optimizer_a/src/glsl-optimizer_a-build/libmesa.a
      )
    
    include_directories(
      ${CHAINBLOCKS_DIR}/deps/bgfx/3rdparty/glsl-optimizer/src/glsl
    )

    add_library(libglsloptimizer STATIC IMPORTED GLOBAL)
    add_dependencies(libglsloptimizer glsl-optimizer_a)
    set_target_properties(libglsloptimizer PROPERTIES IMPORTED_LOCATION ${CMAKE_CURRENT_BINARY_DIR}/glsl-optimizer_a/src/glsl-optimizer_a-build/libglsl_optimizer.a)
    add_library(libglcpp STATIC IMPORTED GLOBAL)
    add_dependencies(libglcpp glsl-optimizer_a)
    set_target_properties(libglcpp PROPERTIES IMPORTED_LOCATION ${CMAKE_CURRENT_BINARY_DIR}/glsl-optimizer_a/src/glsl-optimizer_a-build/libglcpp-library.a)
    add_library(libmesa STATIC IMPORTED GLOBAL)
    add_dependencies(libmesa glsl-optimizer_a)
    set_target_properties(libmesa PROPERTIES IMPORTED_LOCATION ${CMAKE_CURRENT_BINARY_DIR}/glsl-optimizer_a/src/glsl-optimizer_a-build/libmesa.a)
  endif()

  ExternalProject_Add(snappy_a
    GIT_REPOSITORY    https://github.com/chainblocks/snappy.git
    GIT_TAG           563e4e90f4ed6314a14055826f027b2239a8bf0e
    PREFIX ${CMAKE_CURRENT_BINARY_DIR}/snappy_a
    CMAKE_ARGS -DCMAKE_BUILD_TYPE=Release -DSNAPPY_BUILD_TESTS=0 -DCMAKE_INSTALL_PREFIX:PATH=${CMAKE_CURRENT_BINARY_DIR}/snappy_a ${EXTRA_CMAKE_ARGS}
    BUILD_COMMAND cmake --build . --target snappy ${EXTRA_CMAKE_BUILD_ARGS}
    BUILD_BYPRODUCTS ${CMAKE_CURRENT_BINARY_DIR}/snappy_a/lib/libsnappy.a
    BUILD_BYPRODUCTS ${CMAKE_CURRENT_BINARY_DIR}/snappy_a/include/snappy.h
    )

  include_directories(${CMAKE_CURRENT_BINARY_DIR}/snappy_a/include)

  add_library(libsnappy STATIC IMPORTED GLOBAL)
  add_dependencies(libsnappy snappy_a)
  if(IOS)
    set_target_properties(libsnappy PROPERTIES IMPORTED_LOCATION ${CMAKE_CURRENT_BINARY_DIR}/snappy_a/src/snappy_a-build/${EXTRA_LIBS_DIR}/libsnappy.a)
  else()
    set_target_properties(libsnappy PROPERTIES IMPORTED_LOCATION ${CMAKE_CURRENT_BINARY_DIR}/snappy_a/lib/libsnappy.a)
  endif()
  
  set(CB_EXTRAS ${CB_EXTRAS} ${CHAINBLOCKS_DIR}/src/extra/snappy.cpp)
  set_source_files_properties(${CHAINBLOCKS_DIR}/src/extra/snappy.cpp PROPERTIES OBJECT_DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/snappy_a/include/snappy.h)

  ExternalProject_Add(brotli_a
    GIT_REPOSITORY    https://github.com/chainblocks/brotli.git
    GIT_TAG           fcda9db7fd554ffb19c2410b9ada57cdabd19de5
    PREFIX ${CMAKE_CURRENT_BINARY_DIR}/brotli_a
    CMAKE_ARGS -DCMAKE_BUILD_TYPE=Release -DBROTLI_BUNDLED_MODE=1 -DCMAKE_INSTALL_PREFIX:PATH=${CMAKE_CURRENT_BINARY_DIR}/brotli_a ${EXTRA_CMAKE_ARGS}
    BUILD_COMMAND cmake --build . --target brotlicommon-static brotlidec-static brotlienc-static ${EXTRA_CMAKE_BUILD_ARGS}
    BUILD_BYPRODUCTS ${CMAKE_CURRENT_BINARY_DIR}/brotli_a/src/brotli_a-build/libbrotlicommon-static.a
    BUILD_BYPRODUCTS ${CMAKE_CURRENT_BINARY_DIR}/brotli_a/src/brotli_a-build/libbrotlidec-static.a
    BUILD_BYPRODUCTS ${CMAKE_CURRENT_BINARY_DIR}/brotli_a/src/brotli_a-build/libbrotlienc-static.a
    BUILD_BYPRODUCTS ${CMAKE_CURRENT_BINARY_DIR}/brotli_a/src/brotli_a/c/include/brotli/decode.h
    INSTALL_COMMAND ""
    )

  include_directories(${CMAKE_CURRENT_BINARY_DIR}/brotli_a/src/brotli_a/c/include)

  add_library(libbrotlicommon-static STATIC IMPORTED GLOBAL)
  add_dependencies(libbrotlicommon-static brotli_a)
  if(IOS)
    set_target_properties(libbrotlicommon-static PROPERTIES 
      IMPORTED_LOCATION ${CMAKE_CURRENT_BINARY_DIR}/brotli_a/src/brotli_a-build/${EXTRA_LIBS_DIR}/libbrotlicommon-static.a
      )
  else()
    set_target_properties(libbrotlicommon-static PROPERTIES 
      IMPORTED_LOCATION ${CMAKE_CURRENT_BINARY_DIR}/brotli_a/src/brotli_a-build/libbrotlicommon-static.a
      )
  endif()

  add_library(libbrotlidec-static STATIC IMPORTED GLOBAL)
  add_dependencies(libbrotlidec-static brotli_a)
  if(IOS)
    set_target_properties(libbrotlidec-static PROPERTIES 
      IMPORTED_LOCATION ${CMAKE_CURRENT_BINARY_DIR}/brotli_a/src/brotli_a-build/${EXTRA_LIBS_DIR}/libbrotlidec-static.a
      )
  else()
    set_target_properties(libbrotlidec-static PROPERTIES 
      IMPORTED_LOCATION ${CMAKE_CURRENT_BINARY_DIR}/brotli_a/src/brotli_a-build/libbrotlidec-static.a
      )
  endif()

  add_library(libbrotlienc-static STATIC IMPORTED GLOBAL)
  add_dependencies(libbrotlienc-static brotli_a)
  if(IOS)
    set_target_properties(libbrotlienc-static PROPERTIES 
      IMPORTED_LOCATION ${CMAKE_CURRENT_BINARY_DIR}/brotli_a/src/brotli_a-build/${EXTRA_LIBS_DIR}/libbrotlienc-static.a
      )
  else()
    set_target_properties(libbrotlienc-static PROPERTIES 
      IMPORTED_LOCATION ${CMAKE_CURRENT_BINARY_DIR}/brotli_a/src/brotli_a-build/libbrotlienc-static.a
      )
  endif()

  set(CB_EXTRAS ${CB_EXTRAS} ${CHAINBLOCKS_DIR}/src/extra/brotli.cpp)
  set_source_files_properties(${CHAINBLOCKS_DIR}/src/extra/brotli.cpp PROPERTIES OBJECT_DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/brotli_a/src/brotli_a/c/include/brotli/decode.h)

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
    ${CHAINBLOCKS_DIR}/deps/implot/implot_items.cpp

    # shader compiler
    ${CHAINBLOCKS_DIR}/deps/bgfx/tools/shaderc/shaderc.cpp
    ${CHAINBLOCKS_DIR}/deps/bgfx/3rdparty/fcpp/cpp1.c
    ${CHAINBLOCKS_DIR}/deps/bgfx/3rdparty/fcpp/cpp2.c
    ${CHAINBLOCKS_DIR}/deps/bgfx/3rdparty/fcpp/cpp3.c
    ${CHAINBLOCKS_DIR}/deps/bgfx/3rdparty/fcpp/cpp4.c
    ${CHAINBLOCKS_DIR}/deps/bgfx/3rdparty/fcpp/cpp5.c
    ${CHAINBLOCKS_DIR}/deps/bgfx/3rdparty/fcpp/cpp6.c

    ${PLATFORM_SOURCES}
    )

  # build rust blocks
  if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    if(EMSCRIPTEN)
      set(RUST_BUILD_SUBDIR wasm32-unknown-unknown/debug)
      set(RUST_BUILD_OPTIONS ${RUST_BUILD_OPTIONS} --target=wasm32-unknown-unknown)
    elseif(X86_IOS_SIMULATOR)
      set(RUST_BUILD_SUBDIR x86_64-apple-ios/debug)
      set(RUST_BUILD_OPTIONS ${RUST_BUILD_OPTIONS} --target=x86_64-apple-ios)
    elseif(IOS)
      set(RUST_BUILD_SUBDIR aarch64-apple-ios/debug)
      set(RUST_BUILD_OPTIONS ${RUST_BUILD_OPTIONS} --target=aarch64-apple-ios)
    elseif(WIN32 AND CMAKE_SIZEOF_VOID_P EQUAL 4)
      set(RUST_BUILD_SUBDIR i686-pc-windows-gnu/debug)
      set(RUST_BUILD_OPTIONS ${RUST_BUILD_OPTIONS} --target=i686-pc-windows-gnu)
    else()
      set(RUST_BUILD_SUBDIR debug)
    endif()
  else()
    if(EMSCRIPTEN)
      set(RUST_BUILD_SUBDIR wasm32-unknown-unknown/release)    
      set(RUST_BUILD_OPTIONS ${RUST_BUILD_OPTIONS} --release --target=wasm32-unknown-unknown)
    elseif(X86_IOS_SIMULATOR)
      set(RUST_BUILD_SUBDIR x86_64-apple-ios/release)    
      set(RUST_BUILD_OPTIONS ${RUST_BUILD_OPTIONS} --release --target=x86_64-apple-ios)
    elseif(IOS)
      set(RUST_BUILD_SUBDIR aarch64-apple-ios/release)    
      set(RUST_BUILD_OPTIONS ${RUST_BUILD_OPTIONS} --release --target=aarch64-apple-ios)
    elseif(WIN32 AND CMAKE_SIZEOF_VOID_P EQUAL 4)
      set(RUST_BUILD_SUBDIR i686-pc-windows-gnu/release)
      set(RUST_BUILD_OPTIONS ${RUST_BUILD_OPTIONS} --release --target=i686-pc-windows-gnu)
    else()
      set(RUST_BUILD_SUBDIR release)
      set(RUST_BUILD_OPTIONS ${RUST_BUILD_OPTIONS} --release)
    endif()
  endif()

  add_custom_target(
    rust_blocks
    COMMAND cargo build --features cb_static ${RUST_BUILD_OPTIONS}
    WORKING_DIRECTORY ${CHAINBLOCKS_DIR}/rust
  )

  add_library(cb_rust_static STATIC IMPORTED GLOBAL)
  set_target_properties(cb_rust_static PROPERTIES IMPORTED_LOCATION ${CHAINBLOCKS_DIR}/target/${RUST_BUILD_SUBDIR}/libchainblocks.a)
  add_dependencies(cb_rust_static rust_blocks)
endif()

add_compile_definitions(REPLXX_STATIC)
set(replxx-source-patterns ${CHAINBLOCKS_DIR}/deps/replxx/src/*.cpp ${CHAINBLOCKS_DIR}/deps/replxx/src/*.cxx)
file(GLOB replxx-sources ${replxx-source-patterns})

set(RUNTIME_SRC
  ${CHAINBLOCKS_DIR}/deps/easyloggingpp/src/easylogging++.cc
  ${replxx-sources}
  ${CHAINBLOCKS_DIR}/src/core/runtime.cpp
  ${CHAINBLOCKS_DIR}/src/core/ops_internal.cpp
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
  ${CHAINBLOCKS_DIR}/src/core/blocks/bigint.cpp
  ${CHAINBLOCKS_DIR}/src/core/blocks/fs.cpp
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
    ${CHAINBLOCKS_DIR}/src/core/blocks/genetic.cpp
    ${CHAINBLOCKS_DIR}/src/core/blocks/process.cpp
    ${CHAINBLOCKS_DIR}/src/core/blocks/os.cpp
    ${CHAINBLOCKS_DIR}/src/core/blocks/network.cpp
    ${CHAINBLOCKS_DIR}/src/core/blocks/python.cpp
    ${CHAINBLOCKS_DIR}/src/core/blocks/http.cpp
    ${CHAINBLOCKS_DIR}/src/core/blocks/ws.cpp
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
target_link_libraries(cb_shared ${BOOST_LIBS} ${EXTRA_LIBS} ${CB_PLATFORM_LIBS} ${CHAINBLOCKS_EXTRA_LINKS})

if(NOT BUILD_CORE_ONLY)
  add_dependencies(cb_static cb_rust_static) 
  add_dependencies(cb_shared cb_rust_static)
  set(EXTRA_LIBS ${EXTRA_LIBS} cb_rust_static)

  if(NOT EMSCRIPTEN)
    # we link with emscripten SDL in the case of emscripten
    add_dependencies(cb_static libsdl)
    add_dependencies(cb_shared libsdl)
    set(EXTRA_LIBS ${EXTRA_LIBS} libsdl)
  endif()

  add_dependencies(cb_static libsnappy)
  add_dependencies(cb_shared libsnappy)
  set(EXTRA_LIBS ${EXTRA_LIBS} libsnappy)

  add_dependencies(cb_static libbrotlidec-static)
  add_dependencies(cb_shared libbrotlidec-static)
  set(EXTRA_LIBS ${EXTRA_LIBS} libbrotlidec-static)

  add_dependencies(cb_static libbrotlienc-static)
  add_dependencies(cb_shared libbrotlienc-static)
  set(EXTRA_LIBS ${EXTRA_LIBS} libbrotlienc-static)

  add_dependencies(cb_static libbrotlicommon-static)
  add_dependencies(cb_shared libbrotlicommon-static)
  set(EXTRA_LIBS ${EXTRA_LIBS} libbrotlicommon-static)

  if(LINUX)
    add_dependencies(cb_static libglsloptimizer)
    add_dependencies(cb_shared libglsloptimizer)
    set(EXTRA_LIBS ${EXTRA_LIBS} libglsloptimizer)
    add_dependencies(cb_static libglcpp)
    add_dependencies(cb_shared libglcpp)
    set(EXTRA_LIBS ${EXTRA_LIBS} libglcpp)
    add_dependencies(cb_static libmesa)
    add_dependencies(cb_shared libmesa)
    set(EXTRA_LIBS ${EXTRA_LIBS} libmesa)
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
target_link_libraries(cbl ${CB_RUNTIME_LIBS} ${EXTRA_LIBS} ${BOOST_LIBS} ${CB_PLATFORM_LIBS})

if(EMSCRIPTEN)
set_target_properties(cbl PROPERTIES SUFFIX ".html")
endif()
###

### our experimental edn eval
add_executable(
  cbedn
  "${CHAINBLOCKS_DIR}/src/core/edn/main.cpp"
  )
add_dependencies(cbedn cb_static)
include_directories(${CMAKE_CURRENT_BINARY_DIR})
set_property(TARGET cbedn PROPERTY COMPILE_FLAGS "-DCB_DLL_EXPORT")
target_link_libraries(cbedn ${CB_RUNTIME_LIBS} ${EXTRA_LIBS} ${BOOST_LIBS} ${CB_PLATFORM_LIBS})
###
