set(BUILD_SHARED_LIBS OFF)

# Find & cache git using (only considering host tools)
set(CMAKE_FIND_USE_SYSTEM_ENVIRONMENT_PATH ON)
find_package(Git REQUIRED)

include(SHExternalProject)
include(FetchContent)

add_subdirectory(filesystem)
add_subdirectory(json)

add_subdirectory(kcp)
add_library(kcp-wrapper INTERFACE)
target_link_libraries(kcp-wrapper INTERFACE kcp)
target_include_directories(kcp-wrapper INTERFACE kcp)

add_subdirectory(magic_enum)
add_subdirectory(nameof)
add_subdirectory(replxx)
add_subdirectory(spdlog)
add_subdirectory(sqlite)

if(NOT DEFINED CMAKE_POSITION_INDEPENDENT_CODE)
  set(CMAKE_POSITION_INDEPENDENT_CODE OFF CACHE BOOL "" FORCE)
endif()
set(TBB_STRICT OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
add_subdirectory(oneTBB)

if(NOT EMSCRIPTEN)
  if(DESKTOP_LINUX)
    if(GNU_STATIC_BUILD)
      set(OPENSSL_USE_STATIC_LIBS TRUE)
    endif()
    find_package(OpenSSL REQUIRED)
    add_library(OpenSSL INTERFACE)
    target_link_libraries(OpenSSL INTERFACE OpenSSL::SSL)
    target_include_directories(OpenSSL INTERFACE ${OPENSSL_INCLUDE_DIR})

    message(STATUS "Using system OpenSSL: ${OPENSSL_SSL_LIBRARY}, ${OPENSSL_INCLUDE_DIR}")
  else()
    # Force default config path to system's /etc/ssl
    # Otherwise libressl will use the install prefix
    set(OPENSSLDIR "/etc/ssl" CACHE PATH "" FORCE)

    set(LIBRE_SSL_DIR "" CACHE FILEPATH "Use libressl from this path, not setting this will download the sources automatically")
    if(LIBRE_SSL_DIR)
      set(libressl_SOURCE_DIR ${LIBRE_SSL_DIR})

      message(STATUS "Using libressl from LIBRE_SSL_DIR=${libressl_SOURCE_DIR}")
      add_subdirectory(${libressl_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}/libressl)
    else()
      if(CMAKE_GENERATOR STREQUAL Xcode)
        # Required to fix linking against empty libraries with object library dependencies from xcode
        set(LIBRESSL_PATCH_COMMAND PATCH_COMMAND patch -p1 -i ${CMAKE_CURRENT_LIST_DIR}/libressl.patch)
      endif()

      message(STATUS "Fetching libressl")
      FetchContent_Declare(
        libressl
        URL      https://develop.fragcolor.com/libressl-3.4.1.tar.gz
        # URL      https://ftp.openbsd.org/pub/OpenBSD/LibreSSL/libressl-3.4.1.tar.gz
        # URL      https://cloudflare.cdn.openbsd.org/pub/OpenBSD/LibreSSL/libressl-3.4.1.tar.gz
        URL_HASH SHA256=107ceae6ca800e81cb563584c16afa36d6c7138fade94a2b3e9da65456f7c61c
        ${LIBRESSL_PATCH_COMMAND}
      )
      FetchContent_MakeAvailable(libressl)
    endif()

    # Set source dir property used later by rust openssl-sys
    message(STATUS "libressl_SOURCE_DIR=${libressl_SOURCE_DIR}")
    set_property(GLOBAL PROPERTY libressl_SOURCE_DIR ${libressl_SOURCE_DIR})

    # NOTE: External project doesn't work with Xcode here
    #  since ssl/cypto builds into object library referenced by static library
    add_library(OpenSSL INTERFACE)
    add_library(OpenSSL::SSL ALIAS OpenSSL)
    add_library(OpenSSL::Crypto ALIAS crypto)
    target_link_libraries(OpenSSL INTERFACE ssl)
    target_include_directories(OpenSSL INTERFACE ${libressl_SOURCE_DIR}/include)
  endif()
endif()

if(WIN32)
  set(BOOST_STACKTRACE_ENABLE_WINDBG_CACHED ON CACHE BOOL "")
endif()
set(BOOST_DIR "" CACHE FILEPATH "Use boost from this path, not setting this will download the sources automatically")
if(BOOST_DIR)
  message(STATUS "Using boost from BOOST_DIR=${BOOST_DIR}")
  add_subdirectory(${BOOST_DIR} ${CMAKE_CURRENT_BINARY_DIR}/boost)
else()
  message(STATUS "Fetching boost")
  FetchContent_Declare(
    boost
    URL      https://develop.fragcolor.com/boost-1.81.0-shards-5.7z
    # URL      https://github.com/shards-lang/boost/releases/download/boost-1.81.0-shards-5/output.7z
    URL_HASH SHA256=a890f672200cf9f018cbd75a433a4dd7837e09a97b0b9287861a497a4a744626
  )
  FetchContent_MakeAvailable(boost)
  message(STATUS "boost_SOURCE_DIR=${boost_SOURCE_DIR}")
endif()

if(NOT EMSCRIPTEN)
  if(NOT ANDROID AND EXTERNAL_BUILD_TYPE MATCHES "Debug")
    set(SDL_LIB_SUFFIX "d")
  endif()

  if(LIB_SUFFIX STREQUAL ".lib")
    set(SDL2_LIB_NAME "SDL2-static")
  else()
    set(SDL2_LIB_NAME "SDL2")
  endif()

  sh_add_external_project(
    NAME sdl_a
    TARGETS   SDL2 SDL2main
    LIB_NAMES ${SDL2_LIB_NAME} SDL2main
    INSTALL
    LIB_SUFFIX ${SDL_LIB_SUFFIX}
    RELATIVE_INSTALL_INCLUDE_PATHS include/SDL2
    CMAKE_ARGS -DSDL_RENDER_DISABLED=ON -DSDL_OPENGLES=OFF -DBUILD_SHARED_LIBS=OFF
    REPO_ARGS URL ${CMAKE_CURRENT_SOURCE_DIR}/SDL)


  if(APPLE)
    target_link_libraries(SDL2 INTERFACE
      "-framework Foundation"
      "-framework CoreAudio"
      "-framework AudioToolbox"
      "-framework CoreVideo"
      "-framework IOKit"
      "-framework QuartzCore"
      "-framework Metal"
      "-framework Security"
      "-framework CoreHaptics"
      "-framework GameController"
      "-framework CoreBluetooth"
      -liconv
    )

    if(IOS)
      target_link_libraries(SDL2 INTERFACE
        "-framework AVFoundation"
        "-framework CoreMotion"
        "-framework CoreGraphics"
        "-framework UIKit"
      )
    else()
      target_link_libraries(SDL2 INTERFACE
        "-framework Cocoa"
        "-framework Carbon"
        "-framework ForceFeedback"
      )
    endif()
  endif()

  if(WIN32)
    target_link_libraries(SDL2 INTERFACE user32 gdi32 winmm imm32 ole32 oleaut32 version uuid advapi32 setupapi shell32 dinput8)
  elseif(LINUX)
    target_link_libraries(SDL2 INTERFACE dl)
  endif()

  # Required for SDL2main entry point to call main
  if(NOT DESKTOP)
    target_compile_definitions(SDL2main INTERFACE "main=SDL_main")
  else()
    target_compile_definitions(SDL2 INTERFACE "SDL_MAIN_HANDLED=1")
  endif()
else()
  add_library(SDL2 INTERFACE)
  add_library(SDL2main INTERFACE)

  # Use builtin SDL2 port
  target_compile_options(SDL2 INTERFACE
    "-sUSE_SDL=2"
  )
  target_link_options(SDL2 INTERFACE
    # "-sMIN_WEBGL_VERSION=2"
    # "-sMAX_WEBGL_VERSION=2"
    "-sUSE_SDL=2"
  )
endif()

sh_add_external_project(
  NAME wasm3_a
  TARGETS m3
  LIB_RELATIVE_DIRS source/
  CMAKE_ARGS -DBUILD_NATIVE=0
  RELATIVE_INCLUDE_PATHS source
  REPO_ARGS URL ${CMAKE_CURRENT_SOURCE_DIR}/wasm3)

sh_add_external_project(
  NAME snappy_a
  TARGETS snappy
  CMAKE_ARGS -DSNAPPY_BUILD_TESTS=0
  RELATIVE_INCLUDE_PATHS .
  RELATIVE_BINARY_INCLUDE_PATHS .
  REPO_ARGS GIT_REPOSITORY    https://github.com/shards-lang/snappy.git
            GIT_TAG           9e3775b)

sh_add_external_project(
  NAME brotli_a
  TARGETS brotlidec-static brotlienc-static brotlicommon-static
  CMAKE_ARGS -DBROTLI_BUNDLED_MODE=1
  RELATIVE_INCLUDE_PATHS c/include
  REPO_ARGS GIT_REPOSITORY    https://github.com/shards-lang/brotli.git
            GIT_TAG           e83c7b8e8fb8b696a1df6866bc46cbb76d7e0348)


if(CMAKE_BUILD_TYPE MATCHES "Debug")
  set(CATCH2_LIB_SUFFIX "d")
endif()
sh_add_external_project(
  NAME catch2_a
  INSTALL
  TARGETS Catch2 Catch2Main
  LIB_SUFFIX ${CATCH2_LIB_SUFFIX}
  REPO_ARGS URL ${SHARDS_DIR}/deps/Catch2)

sh_add_external_project(
  NAME kissfft_a
  TARGETS kissfft
  LIB_SUFFIX -float
  CMAKE_ARGS -DKISSFFT_PKGCONFIG=0 -DKISSFFT_STATIC=1 -DKISSFFT_TEST=0 -DKISSFFT_TOOLS=0
  RELATIVE_INCLUDE_PATHS .
  REPO_ARGS URL ${SHARDS_DIR}/deps/kissfft)

add_library(tinygltf INTERFACE)
target_include_directories(tinygltf INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

# Draco doesn't expose include paths directly, add them here using a wrapper
set(DRACO_JS_GLUE OFF CACHE BOOL "" FORCE)
add_subdirectory(draco)
add_library(draco-wrapper INTERFACE)
target_link_libraries(draco-wrapper INTERFACE draco::draco)
target_include_directories(draco-wrapper INTERFACE draco/src)
target_include_directories(draco-wrapper INTERFACE ${CMAKE_BINARY_DIR})

set(TF_BUILD_TESTS OFF CACHE BOOL "")
set(TF_BUILD_EXAMPLES OFF CACHE BOOL "")
add_subdirectory(cpp-taskflow)

add_library(xxHash INTERFACE)
target_include_directories(xxHash INTERFACE xxHash)

add_library(linalg INTERFACE)
target_include_directories(linalg INTERFACE linalg ${CMAKE_CURRENT_LIST_DIR})

add_library(miniaudio INTERFACE)
target_include_directories(miniaudio INTERFACE miniaudio)

add_library(stb INTERFACE)
target_include_directories(stb INTERFACE stb)

add_library(utf8.h INTERFACE)
target_include_directories(utf8.h INTERFACE utf8.h)

add_library(pdqsort INTERFACE)
target_include_directories(pdqsort INTERFACE pdqsort)

add_library(entt INTERFACE)
target_include_directories(entt INTERFACE entt/src)

# Tracy profiler, default off
option(TRACY_ENABLE "Enable profiling" OFF)

# Enable fibers support by default
option(TRACY_FIBERS "Enable fibers support" ON)
option(TRACY_CALLSTACK_SIZE "Tracy callstack size" 8)

# These settings are required to make tracy from C++ work together with rust's tracy-client
set(TRACY_MANUAL_LIFETIME ON CACHE BOOL "" FORCE)
set(TRACY_DELAYED_INIT ON CACHE BOOL "" FORCE)
add_subdirectory(tracy)

# Add the wrapper include path that contains tracy/Wrapper.hpp that defines the correct TRACY_CALLSTACK based on the stack size configuration
target_include_directories(TracyClient PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/tracy_wrapper>)
target_compile_definitions(TracyClient PUBLIC TRACY_CALLSTACK_SIZE=${TRACY_CALLSTACK_SIZE})
