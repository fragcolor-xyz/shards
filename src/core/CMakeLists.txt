set(core_SOURCES
  runtime.cpp
  ops_internal.cpp
  number_types.cpp
  blocks/assert.cpp
  blocks/chains.cpp
  blocks/logging.cpp
  blocks/flow.cpp
  blocks/seqs.cpp
  blocks/casting.cpp
  blocks/core.cpp
  blocks/linalg.cpp
  blocks/serialization.cpp
  blocks/json.cpp
  blocks/struct.cpp
  blocks/time.cpp
  blocks/strings.cpp
  blocks/channels.cpp
  blocks/random.cpp
  blocks/bigint.cpp
  blocks/fs.cpp
  blocks/edn.cpp
  blocks/reflection.cpp
  blocks/imaging.cpp
  blocks/wasm.cpp
  blocks/http.cpp
)

set(os_SOURCES
  blocks/process.cpp
  blocks/network.cpp
  blocks/ws.cpp
)

option(CHAINBLOCKS_CORE_BUILD_SHARED OFF "Build chainblocks core as dynamic library")
if(CHAINBLOCKS_CORE_BUILD_SHARED)
  add_library(chainblocks-core SHARED ${core_SOURCES})
  target_compile_definitions(chainblocks-core PUBLIC "-DCHAINBLOCKS_CORE_DLL=1")
else()
  add_library(chainblocks-core STATIC ${core_SOURCES})
endif()

target_include_directories(chainblocks-core PUBLIC ../../include)
target_include_directories(chainblocks-core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

if(EMSCRIPTEN)
  target_compile_options(chainblocks-core PRIVATE "SHELL:-s DISABLE_EXCEPTION_CATCHING=0")
endif()

if(EMSCRIPTEN)
  # emcmake sets this, but can't find boost anymore, see https://github.com/emscripten-core/emscripten/issues/10078
  unset(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE)
  
  set(Boost_DEBUG ON)

  # Only need the headers
  find_package(Boost REQUIRED) 
  
  message(STATUS "Boost_INCLUDE_DIRS ${Boost_INCLUDE_DIRS}")
  if(NOT ${Boost_FOUND})
      message(FATAL_ERROR "Boost was not found")
  endif()
  
  target_include_directories(chainblocks-core PUBLIC ${Boost_INCLUDE_DIRS})
else()
  find_package(Boost REQUIRED COMPONENTS context filesystem)
  target_link_libraries(chainblocks-core Boost::context Boost::filesystem)
endif()

target_link_libraries(chainblocks-core 
  spdlog magic_enum nameof linalg xxHash
  pdqsort utf8.h Taskflow stb nlohmann_json m3 ghc_filesystem 
)

if(NOT EMSCRIPTEN)
  target_sources(chainblocks-core PRIVATE ${os_SOURCES})
else()
  target_sources(chainblocks-core PRIVATE coro.cpp) # emscripten coroutines
endif()

if(WIN32)
  target_link_libraries(chainblocks-core ssl crypto ws2_32 mswsock Secur32 Crypt32 NCrypt)
elseif(APPLE)
  target_link_directories(chainblocks-core PUBLIC 
    /usr/local/lib
    /usr/local/opt/openssl/lib
  )
  target_include_directories(chainblocks-core PUBLIC 
    /usr/local/include
    /usr/local/opt/openssl/include
    /usr/local/include/boost
  )
  target_link_libraries(chainblocks-core ssl crypto)
elseif(UNIX AND NOT EMSCRIPTEN)
  target_link_libraries(chainblocks-core ssl crypto pthread dl rt)
endif()

target_compile_options(chainblocks-core 
  PRIVATE -ffast-math -fno-finite-math-only -funroll-loops
)

target_compile_options(chainblocks-core 
  PUBLIC -Wno-multichar
  PRIVATE -Wno-missing-field-initializers -Wno-deprecated-copy
)

# TODO: Modularize block registration to remove Core=>Rust dependency
if(CHAINBLOCKS_WITH_EXTRA_BLOCKS)
  target_compile_definitions(chainblocks-core PUBLIC "-DCHAINBLOCKS_WITH_EXTRA_BLOCKS=1")
  target_link_libraries(chainblocks-core chainblocks-extra)
endif()

if(EMSCRIPTEN)
  em_link_js_library(chainblocks-core blocks/core.js)
  
  target_compile_options(chainblocks-core PUBLIC 
    "SHELL:-s DISABLE_EXCEPTION_CATCHING=0"
    "SHELL:-s FETCH=1"
    "SHELL:-s MIN_WEBGL_VERSION=2"
    "SHELL:-s MAX_WEBGL_VERSION=2"
  )
  
  target_link_options(chainblocks-core PUBLIC
    "SHELL:-s DISABLE_EXCEPTION_CATCHING=0"
    "SHELL:-s NO_EXIT_RUNTIME=1"
    "SHELL:-s INITIAL_MEMORY=209715200"
    "SHELL:-s FETCH=1"
    "SHELL:-s ALLOW_MEMORY_GROWTH=1"
    "SHELL:-s ASYNCIFY=1"
    "SHELL:-s LLD_REPORT_UNDEFINED"
    "SHELL:-s MODULARIZE=1"
    "SHELL:-s EXPORT_NAME=cbl"
    "SHELL:-s MIN_WEBGL_VERSION=2"
    "SHELL:-s MAX_WEBGL_VERSION=2"
    "SHELL:-s \"EXPORTED_FUNCTIONS=['_main', '_chainblocksInterface', '_malloc', '_free', '_emscripten_get_now']\""
    "SHELL:-s \"ASYNCIFY_IMPORTS=['emEval', 'emCompileShaderBlocking']\""
    "SHELL:-s \"EXPORTED_RUNTIME_METHODS=['FS', 'callMain', 'ENV', 'IDBFS', 'GL', 'PThread', 'setValue', 'getValue', 'lengthBytesUTF8', 'stringToUTF8']\""
  )
endif()

if(IOS)
  target_compile_definitions(chainblocks-core PUBLIC CHAINBLOCKS_NO_HTTP_BLOCKS=1)
endif()

set(cbedn_SOURCES
  edn/main.cpp
  edn/eval.cpp
)
add_executable(cbedn ${cbedn_SOURCES})
target_link_libraries(cbedn chainblocks-core)

if(CHAINBLOCKS_BUILD_TESTS)
  add_executable(test_runtime ../tests/test_runtime.cpp)
  
  target_link_libraries(test_runtime 
    chainblocks-core Catch2WithMain
  )
  catch_discover_tests(test_runtime)
endif()

