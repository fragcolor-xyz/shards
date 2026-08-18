set(BUILD_SHARED_LIBS OFF)

# Find & cache git using (only considering host tools)
set(CMAKE_FIND_USE_SYSTEM_ENVIRONMENT_PATH ON)
find_package(Git REQUIRED)
set(CMAKE_FIND_USE_SYSTEM_ENVIRONMENT_PATH OFF)

include(SHExternalProject)
include(FetchContent)
include(CPM)

add_subdirectory(filesystem)
add_subdirectory(json)

# kcp - only needed when network module is enabled
is_module_enabled(NETWORK_ENABLED network)
if(NETWORK_ENABLED)
  CPMAddPackage(
    NAME kcp
    GITHUB_REPOSITORY shards-lang/kcp
    GIT_TAG 8d6374afed86c212e62a82cfc44234a54bf8b8f6
  )
  add_library(kcp-wrapper INTERFACE)
  target_link_libraries(kcp-wrapper INTERFACE kcp)
  target_include_directories(kcp-wrapper INTERFACE ${kcp_SOURCE_DIR})
endif()

add_subdirectory(magic_enum)
add_subdirectory(nameof)
add_subdirectory(spdlog)
add_subdirectory(sqlite)

set(DEBUG_RENDERER_IN_DISTRIBUTION ON CACHE BOOL "" FORCE)

# Don't enable these ones
option(USE_AVX2 "Enable AVX2" OFF)
option(USE_AVX512 "Enable AVX512" OFF)

# Required by Jolt as we enable RTTI
set(CPP_RTTI_ENABLED ON CACHE BOOL "Enable C++ RTTI" FORCE)

if(EMSCRIPTEN)
  set(CPP_EXCEPTIONS_ENABLED ON CACHE BOOL "Enable C++ exceptions" FORCE) # wasm requires exceptions to be enabled cos we use exceptions
endif()

# This triggers warnings+errors on valgrind build
option(ENABLE_ALL_WARNINGS "Enable all warnings" OFF)

# JoltPhysics - only needed when physics module is enabled
is_module_enabled(PHYSICS_ENABLED physics)
if(PHYSICS_ENABLED)
  # Required by Jolt to build with WASM SIMD
  option(USE_WASM_SIMD "Enable WASM SIMD" ON)

  if(EMSCRIPTEN)
    add_compile_definitions(JPH_NO_FORCE_INLINE)
  endif()

  CPMAddPackage(
    NAME JoltPhysics
    GITHUB_REPOSITORY shards-lang/JoltPhysics
    GIT_TAG 2f94d15c7709e1a111ac7d659b3c31f36c91167b
    SOURCE_SUBDIR Build
  )
endif()

if(NOT DEFINED CMAKE_POSITION_INDEPENDENT_CODE)
  set(CMAKE_POSITION_INDEPENDENT_CODE OFF CACHE BOOL "" FORCE)
endif()

set(TBB_STRICT OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(TBB_DISABLE_HWLOC_AUTOMATIC_SEARCH ON CACHE BOOL "" FORCE)
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
      message(STATUS "Fetching libressl")
      FetchContent_Declare(
        libressl
        URL https://develop.fragcolor.com/libressl-3.9.2.tar.gz
        URL_HASH SHA256=7b031dac64a59eb6ee3304f7ffb75dad33ab8c9d279c847f92c89fb846068f97
      )
      FetchContent_MakeAvailable(libressl)
    endif()

    # Fix iOS endian compatibility - prevent HAVE_ENDIAN_H from being defined on iOS
    # This allows LibreSSL's compat/endian.h to use the Apple-specific code path
    if(APPLE AND (IOS OR CMAKE_OSX_SYSROOT MATCHES ".*iphoneos.*" OR CMAKE_OSX_SYSROOT MATCHES ".*iphonesimulator.*" OR CMAKE_OSX_SYSROOT MATCHES ".*xros.*" OR CMAKE_OSX_SYSROOT MATCHES ".*watchos.*"))
      message(STATUS "Fixing Apple mobile platform endian compatibility by ensuring HAVE_ENDIAN_H is not defined...")
      # Force HAVE_ENDIAN_H to be undefined for iOS so LibreSSL uses its Apple compatibility code
      if(TARGET ssl_obj)
        target_compile_options(ssl_obj PRIVATE -UHAVE_ENDIAN_H)
      endif()
      if(TARGET crypto_obj)
        target_compile_options(crypto_obj PRIVATE -UHAVE_ENDIAN_H)
      endif()
      if(TARGET ssl)
        target_compile_options(ssl PRIVATE -UHAVE_ENDIAN_H)
      endif()
      if(TARGET crypto)
        target_compile_options(crypto PRIVATE -UHAVE_ENDIAN_H)
      endif()
    endif()

    # Set source dir property used later by rust openssl-sys
    message(STATUS "libressl_SOURCE_DIR=${libressl_SOURCE_DIR}")
    set_property(GLOBAL PROPERTY libressl_SOURCE_DIR ${libressl_SOURCE_DIR})

    # NOTE: External project doesn't work with Xcode here
    # since ssl/cypto builds into object library referenced by static library
    add_library(OpenSSL INTERFACE)
    add_library(OpenSSL::SSL ALIAS OpenSSL)
    add_library(OpenSSL::Crypto ALIAS crypto)
    target_link_libraries(OpenSSL INTERFACE ssl)
    target_include_directories(OpenSSL INTERFACE ${libressl_SOURCE_DIR}/include)

  endif()
endif()

if(WIN32)
  if(CMAKE_BUILD_TYPE STREQUAL "Release")
    set(BOOST_STACKTRACE_ENABLE_WINDBG OFF CACHE BOOL "")
    set(BOOST_STACKTRACE_ENABLE_BASIC ON CACHE BOOL "")
  else()
    set(BOOST_STACKTRACE_ENABLE_WINDBG ON CACHE BOOL "")
  endif()
endif()

if(WIN32 OR EMSCRIPTEN)
  # Prevent Boost::uuid from linking against -latomic
  set(BOOST_UUID_LINK_LIBATOMIC OFF CACHE BOOL "" FORCE)
endif()

# Explicitly list Boost libraries to build.
# Without this, Boost builds ALL libraries which is slow and unnecessary.
# Note: context is pulled in as a transitive dependency from asio/beast even if not listed.
set(BOOST_INCLUDE_LIBRARIES
  filesystem lockfree foreach multiprecision atomic thread tti
  beast asio stacktrace container random interprocess uuid process context
  CACHE STRING "Boost libraries to build")

# Disable Boost.Context coroutine support in Asio (we use our own fiber implementation)
set(BOOST_ASIO_DISABLE_BOOST_CONTEXT_FIBER ON CACHE BOOL "" FORCE)
add_compile_definitions(BOOST_ASIO_DISABLE_BOOST_CONTEXT_FIBER)

# For Zig cross-compilation, force Boost.Context to use ucontext implementation
# instead of fcontext assembly. This avoids issues where Boost misdetects the
# architecture (e.g., detects riscv32 as i386). We don't actually use Boost.Context
# anyway - it's only pulled in as a transitive dependency from asio/beast.
# Note: ucontext doesn't work on iOS/Windows, so only enable for Zig cross builds.
if(ZIG_MUSL)
  set(BOOST_CONTEXT_IMPLEMENTATION "ucontext" CACHE STRING "" FORCE)
endif()

set(BOOST_DIR "" CACHE FILEPATH "Use boost from this path, not setting this will download the sources automatically")

if(BOOST_DIR)
  message(STATUS "Using boost from BOOST_DIR=${BOOST_DIR}")
  add_subdirectory(${BOOST_DIR} ${CMAKE_CURRENT_BINARY_DIR}/boost)
else()
  message(STATUS "Fetching boost")
  FetchContent_Declare(
    boost
    URL https://develop.fragcolor.com/boost-1.87.0.7z
    URL_HASH SHA256=abade069520e065e28740ba1f379a356a96bebb14e1b4b853316587c860747b5
  )

  FetchContent_MakeAvailable(boost)
  message(STATUS "boost_SOURCE_DIR=${boost_SOURCE_DIR}")
endif()

# SDL3 - download for all platforms, but only build for non-emscripten/non-watchOS
# Emscripten needs SDL3 headers for structures and enums even though it doesn't use the library
# Zig cross-compilation also only uses headers (no GUI support when cross-compiling)
if(NOT SHARDS_NO_SDL)
  if(EMSCRIPTEN OR WATCHOS OR ZIG_CROSSCOMPILE)
    # Download headers only for emscripten and watchOS
    CPMAddPackage(
      NAME SDL3
      GITHUB_REPOSITORY shards-lang/SDL-org
      GIT_TAG 6a44ecc618118b2da5414024abf00b6b1f6392b7
      DOWNLOAD_ONLY YES
    )
    # Create interface library for headers
    add_library(SDL3-headers INTERFACE)
    target_include_directories(SDL3-headers INTERFACE ${SDL3_SOURCE_DIR}/include)
    # Cache SDL3_SOURCE_DIR for use by Rust build scripts
    set(SDL3_SOURCE_DIR ${SDL3_SOURCE_DIR} CACHE INTERNAL "SDL3 source directory")
  else()
    # Build SDL3 for other platforms
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)

    if(LINUX)
      # disable pipewire as it has some build issues with GCC 15
      set(SDL_PIPEWIRE OFF CACHE BOOL "" FORCE)
      set(SDL_X11 ON CACHE BOOL "" FORCE)
      set(SDL_WAYLAND ON CACHE BOOL "" FORCE)
      set(SDL_OFFSCREEN OFF CACHE BOOL "" FORCE)
    endif()

    CPMAddPackage(
      NAME SDL3
      GITHUB_REPOSITORY shards-lang/SDL-org
      GIT_TAG 6a44ecc618118b2da5414024abf00b6b1f6392b7
    )
    # Cache SDL3_SOURCE_DIR for use by Rust build scripts
    set(SDL3_SOURCE_DIR ${SDL3_SOURCE_DIR} CACHE INTERNAL "SDL3 source directory")
  endif()
endif()

# snappy - only needed when snappy module is enabled
is_module_enabled(SNAPPY_ENABLED snappy)
if(SNAPPY_ENABLED)
  CPMAddPackage(
    NAME snappy
    GITHUB_REPOSITORY shards-lang/snappy
    GIT_TAG a4476e4764ed3c254cf52a9e23ae159bc9a43a7c
    OPTIONS
      "SNAPPY_BUILD_TESTS OFF"
      "SNAPPY_BUILD_BENCHMARKS OFF"
  )
endif()

# brotli - only needed when brotli module is enabled
is_module_enabled(BROTLI_ENABLED brotli)
if(BROTLI_ENABLED)
  CPMAddPackage(
    NAME brotli
    GITHUB_REPOSITORY shards-lang/brotli
    GIT_TAG ed738e842d2fbdf2d6459e39267a633c4a9b2f5d
    OPTIONS
      "BROTLI_DISABLE_TESTS ON"
  )
endif()

add_subdirectory(Catch2)

# kissfft and opus - only needed when audio module is enabled
is_module_enabled(AUDIO_ENABLED audio)
if(AUDIO_ENABLED)
  # kissfft configuration
  set(KISSFFT_PKGCONFIG OFF CACHE BOOL "" FORCE)
  set(KISSFFT_STATIC ON CACHE BOOL "" FORCE)
  set(KISSFFT_TEST OFF CACHE BOOL "" FORCE)
  set(KISSFFT_TOOLS OFF CACHE BOOL "" FORCE)

  CPMAddPackage(
    NAME kissfft
    GITHUB_REPOSITORY shards-lang/kissfft
    GIT_TAG 8f47a67f595a6641c566087bf5277034be64f24d
  )

  # opus configuration
  set(OPUS_CUSTOM_MODES ON CACHE BOOL "" FORCE)
  set(OPUS_BUILD_FRAMEWORK OFF CACHE BOOL "" FORCE)
  set(OPUS_BUILD_SHARED_LIBRARY OFF CACHE BOOL "" FORCE)
  set(OPUS_BUILD_PROGRAMS OFF CACHE BOOL "" FORCE)
  set(OPUS_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  set(OPUS_ENABLE_FLOAT_API ON CACHE BOOL "" FORCE)

  CPMAddPackage(
    NAME opus
    GITHUB_REPOSITORY xiph/opus
    GIT_TAG 08bcc6e46227fca01aa3de3f3512f8b692d8d36b
  )

  # SLEEF - vectorized math library for audio processing (non-Apple, non-WASM only)
  # Apple uses Accelerate framework, WASM uses scalar fallback
  # Zig cross-compilation: sleef has host tool build issues, use scalar fallback
  if(NOT APPLE AND NOT EMSCRIPTEN AND NOT ZIG_CROSSCOMPILE)
    set(SLEEF_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(SLEEF_BUILD_DFT OFF CACHE BOOL "" FORCE)
    set(SLEEF_BUILD_QUAD OFF CACHE BOOL "" FORCE)
    set(SLEEF_BUILD_GNUABI_LIBS OFF CACHE BOOL "" FORCE)
    set(SLEEF_BUILD_SCALAR_LIB OFF CACHE BOOL "" FORCE)
    set(SLEEF_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

    CPMAddPackage(
      NAME sleef
      GITHUB_REPOSITORY shibatch/sleef
      GIT_TAG 3.7
    )
  endif()
endif()

if(NOT SHARDS_NO_SDL)
  # tinygltf is needed by gfx module pch.cpp
  CPMAddPackage(
    NAME tinygltf
    GITHUB_REPOSITORY shards-lang/tinygltf
    GIT_TAG 967c98dd90406cb9d25df0d95909deae7771e997
    DOWNLOAD_ONLY YES
  )
  add_library(tinygltf INTERFACE)
  target_include_directories(tinygltf INTERFACE ${tinygltf_SOURCE_DIR} ${CMAKE_BINARY_DIR}/_deps)

  # Draco - build for all platforms including emscripten
  set(DRACO_JS_GLUE OFF CACHE BOOL "" FORCE)
  CPMAddPackage(
    NAME draco
    GITHUB_REPOSITORY shards-lang/draco
    GIT_TAG b698ce9b18032a073de66d8b96c91fbde1f8a38b
  )
  add_library(draco-wrapper INTERFACE)
  target_link_libraries(draco-wrapper INTERFACE draco::draco)
  target_include_directories(draco-wrapper INTERFACE ${draco_SOURCE_DIR}/src)
  target_include_directories(draco-wrapper INTERFACE ${CMAKE_BINARY_DIR})
endif()

set(TF_BUILD_TESTS OFF CACHE BOOL "")
set(TF_BUILD_EXAMPLES OFF CACHE BOOL "")
CPMAddPackage(
  NAME cpp-taskflow
  GITHUB_REPOSITORY shards-lang/cpp-taskflow
  GIT_TAG e6a834c8bf4015cd62c71e829e48d715a60bdb7d
)

add_library(xxHash INTERFACE)
target_include_directories(xxHash INTERFACE xxHash)

CPMAddPackage(
  NAME linalg
  GITHUB_REPOSITORY shards-lang/linalg
  GIT_TAG 4a73b3ecfadcb64e5f9bc59c8f4f14a32d953e0a
  DOWNLOAD_ONLY YES
)
add_library(linalg INTERFACE)
target_include_directories(linalg INTERFACE ${linalg_SOURCE_DIR} ${CMAKE_BINARY_DIR}/_deps)

# miniaudio - only needed when audio module is enabled
is_module_enabled(AUDIO_ENABLED audio)
if(AUDIO_ENABLED)
  CPMAddPackage(
    NAME miniaudio
    GITHUB_REPOSITORY shards-lang/miniaudio
    GIT_TAG 2774449deb737cffac13e3c84f267f8c8629351e
    DOWNLOAD_ONLY YES
  )
  add_library(miniaudio INTERFACE)
  target_include_directories(miniaudio INTERFACE ${miniaudio_SOURCE_DIR} ${CMAKE_BINARY_DIR}/_deps)
endif()

CPMAddPackage(
  NAME stb
  GITHUB_REPOSITORY shards-lang/stb
  GIT_TAG 8ad9b97ecb537c0aedcce58af47d40bf3cdb3ede
  DOWNLOAD_ONLY YES
)
add_library(stb INTERFACE)
target_include_directories(stb INTERFACE ${stb_SOURCE_DIR})

CPMAddPackage(
  NAME utf8.h
  GITHUB_REPOSITORY shards-lang/utf8.h
  GIT_TAG ee5a7d4beb7755da13e4d4ec3eccfb65a0530456
  DOWNLOAD_ONLY YES
)
add_library(utf8.h INTERFACE)
target_include_directories(utf8.h INTERFACE ${utf8.h_SOURCE_DIR} ${CMAKE_BINARY_DIR}/_deps)

CPMAddPackage(
  NAME pdqsort
  GITHUB_REPOSITORY shards-lang/pdqsort
  GIT_TAG 978bc36a9bd4143a54b2551cfd9ce8a6afd6d04c
  DOWNLOAD_ONLY YES
)
add_library(pdqsort INTERFACE)
target_include_directories(pdqsort INTERFACE ${pdqsort_SOURCE_DIR})

CPMAddPackage(
  NAME entt
  GITHUB_REPOSITORY shards-lang/entt
  GIT_TAG 4a2d1a8541228a90e02a873dba6a980506c42c03
  DOWNLOAD_ONLY YES
)

# Create symlinks for CPM-downloaded libraries that need directory-prefixed includes
# This allows includes like <linalg/linalg.h> to work with CPM's -src suffix naming
set(CPM_SYMLINK_LIBS linalg utf8.h entt)
if(NOT SHARDS_NO_SDL)
  list(APPEND CPM_SYMLINK_LIBS tinygltf)
endif()
if(AUDIO_ENABLED)
  list(APPEND CPM_SYMLINK_LIBS miniaudio)
endif()
foreach(LIB ${CPM_SYMLINK_LIBS})
  set(SYMLINK_TARGET ${CMAKE_BINARY_DIR}/_deps/${LIB})
  if(NOT EXISTS ${SYMLINK_TARGET})
    # Use CPM's SOURCE_DIR variable which works with both local and global cache
    file(CREATE_LINK ${${LIB}_SOURCE_DIR} ${SYMLINK_TARGET} SYMBOLIC)
  endif()
endforeach()

add_library(entt INTERFACE)
target_include_directories(entt INTERFACE ${entt_SOURCE_DIR}/src ${CMAKE_BINARY_DIR}/_deps)

# Tracy profiler, default off
option(TRACY_ENABLE "Enable profiling" OFF)

if(TRACY_ENABLE)
  # Enable fibers support by default
  option(TRACY_FIBERS "Enable fibers support" ON)
  option(TRACY_CALLSTACK_SIZE "Tracy callstack size" 8)

  # These settings are required to make tracy from C++ work together with rust's tracy-client
  set(TRACY_MANUAL_LIFETIME ON CACHE BOOL "" FORCE)
  set(TRACY_DELAYED_INIT ON CACHE BOOL "" FORCE)

  CPMAddPackage(
    NAME tracy
    GITHUB_REPOSITORY shards-lang/tracy
    GIT_TAG 82143d48a827e8b25c79505570e290bd3be7191a
  )

  # Add the wrapper include path that contains tracy/Wrapper.hpp that defines the correct TRACY_CALLSTACK based on the stack size configuration
  target_include_directories(TracyClient PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/tracy_wrapper>)
  target_compile_definitions(TracyClient PUBLIC TRACY_CALLSTACK_SIZE=${TRACY_CALLSTACK_SIZE})
else()
  # Create stub TracyClient library when profiling is disabled
  add_library(TracyClient INTERFACE)
  add_library(Tracy::TracyClient ALIAS TracyClient)
  target_include_directories(TracyClient INTERFACE ${CMAKE_CURRENT_LIST_DIR}/tracy_stub)
endif()

# llama.cpp and whisper.cpp - provides LLM.* and Whisper.* shards (Vulkan/Metal/CUDA)
# See also shards/modules/candle for AI.* shards (mistral.rs/candle, Metal/CUDA)
is_module_enabled(LLM_ENABLED llm)
if(LLM_ENABLED AND NOT EMSCRIPTEN AND NOT WATCHOS)
  # llama.cpp configuration
  # GGML_VULKAN can be enabled via -DGGML_VULKAN=ON at cmake configure time.
  # Requires Vulkan SDK installed (glslc shader compiler + vulkan headers/libs).

  set(GGML_BUILD_NUMBER 1)
  set(GGML_STATIC ON CACHE BOOL "" FORCE)
  set(GGML_NATIVE OFF CACHE BOOL "" FORCE)
  set(GGML_AVX ON CACHE BOOL "" FORCE)
  set(GGML_AVX2 ON CACHE BOOL "" FORCE)
  set(GGML_F16C ON CACHE BOOL "" FORCE)
  set(GGML_FMA ON CACHE BOOL "" FORCE)
  set(GGML_OPENMP OFF CACHE BOOL "" FORCE) # omp is very omp...

  if(APPLE)
    if(IS_SIMULATOR)
      set(GGML_METAL OFF CACHE BOOL "" FORCE)
    else()
      set(GGML_METAL ON CACHE BOOL "" FORCE)
      set(GGML_METAL_USE_BF16 ON CACHE BOOL "" FORCE)
      set(GGML_METAL_EMBED_LIBRARY ON CACHE BOOL "" FORCE)
    endif()
  endif()

  # Force math library to be empty, sometimes picks up libm.a from mingw64
  if(WIN32)
    set(MATH_LIBRARY "" CACHE FILEPATH "" FORCE)
  endif()

  # Build the upstream llama-common helper library so we can link against it
  # instead of cherry-picking common/*.cpp sources. Cherry-picking is fragile:
  # common.cpp/sampling.cpp reference symbols in sibling files (fit, speculative,
  # build-info, ngram-*, ...) and the set keeps growing across releases.
  set(LLAMA_BUILD_COMMON ON CACHE BOOL "" FORCE)
  set(LLAMA_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  set(LLAMA_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
  set(LLAMA_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(LLAMA_BUILD_SERVER OFF CACHE BOOL "" FORCE)
  set(LLAMA_OPENSSL OFF CACHE BOOL "" FORCE)        # we never make HTTPS calls from llama-common

  CPMAddPackage(
    NAME llama.cpp
    GITHUB_REPOSITORY ggml-org/llama.cpp
    GIT_TAG b10472
  )

  # llama.cpp >= b9019 uses dynamic_cast<llama_model_base*> in src/llama.cpp and
  # src/llama-quant.cpp (PR #22004). Upstream offers no toggle, so re-enable RTTI
  # for the targets that use it. Mixing RTTI / -fno-rtti TUs is safe here: we
  # never use typeid() across the boundary, and llama / llama-common are static
  # libraries so there are no shared-typeinfo ODR concerns.
  foreach(_t llama llama-common llama-common-base)
    if(TARGET ${_t})
      target_compile_options(${_t} PRIVATE -frtti)
    endif()
  endforeach()

  # whisper.cpp configuration
  set(WHISPER_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  set(WHISPER_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

  CPMAddPackage(
    NAME whisper.cpp
    GITHUB_REPOSITORY ggml-org/whisper.cpp
    GIT_TAG v1.9.2
  )
endif()

if(WIN32)
  set(JPEG_LIB_NAMES jpeg-static)
else()
  set(JPEG_LIB_NAMES jpeg)
endif()

unset(MOZ_OPTIONS)

if(EMSCRIPTEN)
  list(APPEND MOZ_OPTIONS -DREQUIRE_SIMD=OFF -DWITH_SIMD=OFF)
endif()

# mozjpeg - only needed when imaging module is enabled
# Note: mozjpeg doesn't support add_subdirectory, so we use CPM to download
# and sh_add_external_project to build with ExternalProject_Add
is_module_enabled(IMAGING_ENABLED imaging)
if(IMAGING_ENABLED)
  # Download mozjpeg using CPM
  CPMAddPackage(
    NAME mozjpeg
    GITHUB_REPOSITORY shards-lang/mozjpeg
    GIT_TAG 9b8d11f05e3ae4541ce5251f0e5c20c4cb8733b7
    DOWNLOAD_ONLY YES
  )

  # Build using ExternalProject_Add via sh_add_external_project
  sh_add_external_project(
    NAME mozjpeg_a
    LIB_NAMES ${JPEG_LIB_NAMES}
    TARGETS jpeg-static
    RELATIVE_INCLUDE_PATHS .
    RELATIVE_BINARY_INCLUDE_PATHS .
    CMAKE_ARGS -DBUILD_SHARED_LIBS=OFF -DPNG_SUPPORTED=OFF ${MOZ_OPTIONS}
    REPO_ARGS URL ${mozjpeg_SOURCE_DIR})
endif()

# crdt-lite - only needed when crdts module is enabled
is_module_enabled(CRDTS_ENABLED crdts)
if(CRDTS_ENABLED)
  CPMAddPackage(
    NAME crdt-lite
    GITHUB_REPOSITORY sinkingsugar/crdt-lite
    GIT_TAG 1182f85df7a8b371d174a6b3875d54ab496b4c28
    DOWNLOAD_ONLY YES
  )
  add_library(crdt_lite INTERFACE)
  target_include_directories(crdt_lite INTERFACE ${crdt-lite_SOURCE_DIR})
endif()
