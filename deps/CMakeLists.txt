set(BUILD_SHARED_LIBS OFF)

# Find & cache git using (only considering host tools)
set(CMAKE_FIND_USE_SYSTEM_ENVIRONMENT_PATH ON)
find_package(Git REQUIRED)

include(SHExternalProject)
include(FetchContent)

add_subdirectory(filesystem)
add_subdirectory(json)
add_subdirectory(kcp)
add_subdirectory(magic_enum)
add_subdirectory(nameof)
add_subdirectory(pareto)
add_subdirectory(replxx)
add_subdirectory(spdlog)

if(NOT EMSCRIPTEN)
  if(DESKTOP_LINUX)
    find_package(OpenSSL REQUIRED)
    add_library(OpenSSL INTERFACE)
    target_link_libraries(OpenSSL INTERFACE OpenSSL::SSL)
    target_include_directories(OpenSSL INTERFACE ${OPENSSL_INCLUDE_DIR})

    message(STATUS "Using system OpenSSL: ${OPENSSL_SSL_LIBRARY}, ${OPENSSL_INCLUDE_DIR}")
  else()
    # Force default config path to system's /etc/ssl
    # Otherwise libressl will use the install prefix
    set(OPENSSLDIR "/etc/ssl" CACHE PATH "" FORCE)

    if(CMAKE_GENERATOR STREQUAL Xcode)
      # Required to fix linking against empty libraries with object library dependencies from xcode
      set(LIBRESSL_PATCH_COMMAND PATCH_COMMAND patch -p1 -i ${CMAKE_CURRENT_LIST_DIR}/libressl.patch)
    endif()

    message(STATUS "Fetching libressl")
    FetchContent_Declare(
      libressl
      URL      https://ftp.openbsd.org/pub/OpenBSD/LibreSSL/libressl-3.4.1.tar.gz
      URL_HASH SHA256=107ceae6ca800e81cb563584c16afa36d6c7138fade94a2b3e9da65456f7c61c
      ${LIBRESSL_PATCH_COMMAND}
    )
    FetchContent_MakeAvailable(libressl)
    message(STATUS "libressl_SOURCE_DIR=${libressl_SOURCE_DIR}")

    # NOTE: External project doesn't work with Xcode here
    #  since ssl/cypto builds into object library referenced by static library
    add_library(OpenSSL INTERFACE)
    target_link_libraries(OpenSSL INTERFACE ssl)
    target_include_directories(OpenSSL INTERFACE ${libressl_SOURCE_DIR}/include)

    # Set source dir property used later by rust openssl-sys
    set_property(GLOBAL PROPERTY libressl_SOURCE_DIR ${libressl_SOURCE_DIR})
  endif()
endif()

set(BOOST_DIR "" CACHE FILEPATH "Use boost from this path, not setting this will download the sources automatically")
if(BOOST_DIR)
  message(STATUS "Using boost from BOOST_DIR=${BOOST_DIR}")
  add_subdirectory(${BOOST_DIR} ${CMAKE_CURRENT_BINARY_DIR}/boost)
  message(STATUS "boost_SOURCE_DIR=${boost_SOURCE_DIR}")
else()
  message(STATUS "Fetching boost")
  FetchContent_Declare(
    boost
    URL      https://github.com/shards-lang/boost/releases/download/boost-f74a225c09-2/output.7z
    URL_HASH SHA256=7C876AC31F7D1A80C9B4671FD960399F86CF24DC9DA83AFF7F1CBC8984424080
  )
  FetchContent_MakeAvailable(boost)
  message(STATUS "boost_SOURCE_DIR=${boost_SOURCE_DIR}")
endif()

if(NOT EMSCRIPTEN)
  if(NOT ANDROID AND EXTERNAL_BUILD_TYPE MATCHES "Debug")
    set(SDL_LIB_SUFFIX "d")
  endif()

  sh_add_external_project(
    NAME sdl_a
    TARGETS   SDL2 SDL2main
    LIB_NAMES SDL2 SDL2main
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
    "SHELL:-s USE_SDL=2"
  )
  target_link_options(SDL2 INTERFACE
    "SHELL:-s MIN_WEBGL_VERSION=2"
    "SHELL:-s MAX_WEBGL_VERSION=2"
    "SHELL:-s USE_SDL=2"
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
            GIT_TAG           563e4e90f4ed6314a14055826f027b2239a8bf0e)

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

option(TF_BUILD_TESTS "" OFF)
option(TF_BUILD_EXAMPLES "" OFF)
add_subdirectory(cpp-taskflow)

add_library(xxHash INTERFACE)
target_include_directories(xxHash INTERFACE xxHash)

add_library(linalg INTERFACE)
target_include_directories(linalg INTERFACE linalg)

add_library(miniaudio INTERFACE)
target_include_directories(miniaudio INTERFACE miniaudio)

add_library(stb INTERFACE)
target_include_directories(stb INTERFACE stb)

add_library(utf8.h INTERFACE)
target_include_directories(utf8.h INTERFACE utf8.h)

add_library(pdqsort INTERFACE)
target_include_directories(pdqsort INTERFACE pdqsort)
