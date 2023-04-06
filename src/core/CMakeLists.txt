set(core_SOURCES
  async.cpp
  ops_internal.cpp
  number_types.cpp
  shards/assert.cpp
  shards/wires.cpp
  shards/logging.cpp
  shards/flow.cpp
  shards/seqs.cpp
  shards/casting.cpp
  shards/linalg.cpp
  shards/serialization.cpp
  shards/json.cpp
  shards/struct.cpp
  shards/time.cpp
  shards/strings.cpp
  shards/channels.cpp
  shards/random.cpp
  shards/fs.cpp
  shards/edn.cpp
  shards/reflection.cpp
  shards/imaging.cpp
  shards/wasm.cpp
  shards/http.cpp
  shards/events.cpp
  shards/sqlite.cpp
)

set(os_SOURCES
  shards/process.cpp
)

set(network_SOURCES
  shards/network.cpp
)

add_library(shards-core-static STATIC ${core_SOURCES})

target_include_directories(shards-core-static PUBLIC ../../include)
target_include_directories(shards-core-static PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})


option(SHARDS_MEMORY_TRACKING "Enable memory tracking that works with tracy" ON)
if(TRACY_ENABLE AND SHARDS_MEMORY_TRACKING)
  target_compile_definitions(shards-core-static PUBLIC SHARDS_TRACKING=1)
  target_sources(shards-core-static PRIVATE tracking.cpp)
endif()

if(SHARDS_NO_BIGINT_SHARDS)
  target_compile_definitions(shards-core-static PUBLIC SHARDS_NO_BIGINT_SHARDS=1)
else()
  target_sources(shards-core-static PRIVATE shards/bigint.cpp)
endif()

if(CMAKE_BUILD_TYPE MATCHES Debug)
  set(SHARDS_INLINE_EVERYTHING_DEFAULT OFF)
else()
  set(SHARDS_INLINE_EVERYTHING_DEFAULT ON)
endif()
if(NOT DEFINED SHARDS_INLINE_EVERYTHING)
  set(SHARDS_INLINE_EVERYTHING ${SHARDS_INLINE_EVERYTHING_DEFAULT} CACHE BOOL "When enabled, runtime and core shards are inlined in a single compilation unit")
endif()

if(SHARDS_INLINE_EVERYTHING)
  # Compile only runtime_inlined which will compile the runtime and all inlined shards in a single translation unit
  target_compile_definitions(shards-core-static PUBLIC SHARDS_INLINE_EVERYTHING=1)
  target_sources(shards-core-static PRIVATE runtime_inlined.cpp)
else()
  # Compile normally inlined sources separately
  target_sources(shards-core-static PRIVATE
    shards/inlined.cpp
    shards/core.cpp
    shards/math.cpp
    runtime.cpp
  )
endif()

target_precompile_headers(shards-core-static PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/pch.cpp>")

if(APPLE)
  target_link_libraries(shards-core-static
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
    -liconv
  )

  if(IOS)
    target_link_libraries(shards-core-static
      "-framework AVFoundation"
      "-framework CoreMotion"
      "-framework CoreGraphics"
      "-framework UIKit"
    )
  else()
    target_link_libraries(shards-core-static
      "-framework Cocoa"
      "-framework Carbon"
      "-framework ForceFeedback"
    )
  endif()
endif()

target_link_libraries(shards-core-static Boost::filesystem Boost::lockfree Boost::foreach Boost::multiprecision Boost::atomic)

if(NOT EMSCRIPTEN)
  target_link_libraries(shards-core-static Boost::beast Boost::asio Boost::context)
else()
  target_include_directories(shards-core-static PUBLIC $<TARGET_PROPERTY:Boost::asio,INTERFACE_INCLUDE_DIRECTORIES>)
  # For usage of boost/beast/core/detail/base64.hpp
  target_include_directories(shards-core-static PUBLIC $<TARGET_PROPERTY:Boost::beast,INTERFACE_INCLUDE_DIRECTORIES>)
endif()

if(WIN32)
  target_link_libraries(shards-core-static Boost::stacktrace_windbg_cached)

  # Target windows SDK (https://docs.microsoft.com/en-us/cpp/porting/modifying-winver-and-win32-winnt)
  # 0x0A00 = Windows 10
  target_compile_definitions(shards-core-static PUBLIC WINVER=0x0A00 _WIN32_WINNT=0x0A00)
elseif(DESKTOP_LINUX)
  target_link_libraries(shards-core-static Boost::stacktrace_addr2line)
else()
  target_link_libraries(shards-core-static Boost::stacktrace_basic)
endif()

target_link_libraries(shards-core-static
  spdlog magic_enum nameof linalg xxHash sqlite-static
  pdqsort entt utf8.h Taskflow stb nlohmann_json m3 ghc_filesystem
  shards-logging
)

if(DESKTOP)
  target_sources(shards-core-static PRIVATE ${os_SOURCES})
  target_link_libraries(shards-core-static Boost::process kcp)
  target_compile_definitions(shards-core-static PUBLIC SHARDS_DESKTOP=1)
endif()

if(EMSCRIPTEN)
  target_sources(shards-core-static PRIVATE coro.cpp) # emscripten coroutines
endif()

if(NOT EMSCRIPTEN)
  target_sources(shards-core-static PRIVATE ${network_SOURCES})
  target_link_libraries(shards-core-static OpenSSL kcp)
endif()

if(WIN32)
  target_link_libraries(shards-core-static ws2_32 mswsock Secur32 Crypt32 NCrypt)
elseif(DESKTOP_LINUX)
  target_link_libraries(shards-core-static pthread dl rt)
endif()

target_compile_options(shards-core-static
  PRIVATE -ffast-math -fno-finite-math-only -funroll-loops
)

# TODO: Modularize shard registration to remove Core=>Rust dependency
if(SHARDS_WITH_EXTRA_SHARDS)
  target_compile_definitions(shards-core-static PUBLIC "-DSHARDS_WITH_EXTRA_SHARDS=1")
  target_link_libraries(shards-core-static shards-extra)
endif()

if(EMSCRIPTEN)
  set(FETCH_SUPPORTED ON)
  if(NODEJS)
    message(STATUS "Building shards-core for node.js, disabling Fetch support")
    set(FETCH_SUPPORTED OFF)
  endif()

  em_link_js_library(shards-core-static shards/core.js)

  target_compile_options(shards-core-static PUBLIC
    "SHELL:-s DISABLE_EXCEPTION_CATCHING=0"
  )

  if(FETCH_SUPPORTED)
    target_link_options(shards-core-static PUBLIC
      "SHELL:-s FETCH=1")
  else()
    set(NO_HTTP_SHARDS ON)
  endif()

  target_link_options(shards-core-static PUBLIC
    "SHELL:-s DISABLE_EXCEPTION_CATCHING=0"
    "SHELL:-s INITIAL_MEMORY=209715200"
    "SHELL:-s STACK_SIZE=1000000"
    "SHELL:-s ALLOW_MEMORY_GROWTH=1"
    "SHELL:-s LLD_REPORT_UNDEFINED"
    "SHELL:-s \"EXPORTED_FUNCTIONS=['_main', '_shardsInterface', '_malloc', '_free', '_emscripten_get_now']\""
    "SHELL:-s \"ASYNCIFY_IMPORTS=['emEval']\""
    "SHELL:-s \"EXPORTED_RUNTIME_METHODS=['FS', 'callMain', 'ENV', 'IDBFS', 'PThread', 'setValue', 'getValue', 'lengthBytesUTF8', 'stringToUTF8']\""
  )

  target_link_options(shards-core-static PUBLIC "SHELL:-s ASYNCIFY=1")

  # Require optimizations when building with asyncify too keep size/build time reasonable
  if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(shards-core-static PUBLIC "-O3")
    target_link_options(shards-core-static PUBLIC "-O3")
  endif()
endif()

if(IOS)
  set(NO_HTTP_SHARDS ON)
endif()

if(NO_HTTP_SHARDS)
  message(STATUS "Building shards-core without HTTP shards")
  target_compile_definitions(shards-core-static PUBLIC SHARDS_NO_HTTP_SHARDS=1)
endif()

duplicate_library_target(shards-core-static SHARED shards-core-shared)
target_compile_definitions(shards-core-shared PUBLIC SHARDS_CORE_DLL=1 shards_core_EXPORTS=1)

add_library(shards-core ALIAS shards-core-static)

set(shedn_SOURCES
  edn/main.cpp
  edn/eval.cpp
)
add_executable(shedn ${shedn_SOURCES})
target_link_libraries(shedn shards-core)

if(SHARDS_BUILD_TESTS)
  add_executable(test-runtime ../tests/test_runtime.cpp)

  target_link_libraries(test-runtime
    shards-core Catch2 Catch2Main
  )

  if(EMSCRIPTEN)
    target_link_options(test-runtime PUBLIC
      "SHELL:-s MODULARIZE=1"
      "SHELL:-s EXPORT_NAME=test"
      "SHELL:-s NO_EXIT_RUNTIME=1"
    )
  endif()
endif()
