set(core_SOURCES
  runtime.cpp
  ops_internal.cpp
  number_types.cpp
  runtime.cpp
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

add_library(chainblocks-core-static STATIC ${core_SOURCES})

target_include_directories(chainblocks-core-static PUBLIC ../../include)
target_include_directories(chainblocks-core-static PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

if(EMSCRIPTEN)
  target_compile_options(chainblocks-core-static PRIVATE "SHELL:-s DISABLE_EXCEPTION_CATCHING=0")
endif()

if(CHAINBLOCKS_NO_BIGINT_BLOCKS)
  target_compile_definitions(chainblocks-core-static PUBLIC CHAINBLOCKS_NO_BIGINT_BLOCKS=1)
else()
  target_sources(chainblocks-core-static PRIVATE blocks/bigint.cpp)
endif()

if(APPLE)
  target_link_libraries(chainblocks-core-static
    "-framework Foundation"
    "-framework CoreAudio"
    "-framework AudioToolbox"
    "-framework CoreVideo"
    "-framework IOKit"
    "-framework QuartzCore"
    "-framework Metal"
    "-framework Security"
    -liconv
  )

  if(IOS)
    target_link_libraries(chainblocks-core-static
      "-framework hidapi"
      "-framework AVFoundation"
      "-framework GameController"
      "-framework CoreMotion"
      "-framework CoreGraphics"
      "-framework CoreHaptics"
      "-framework UIKit"
    )
  else()
    target_link_libraries(chainblocks-core-static
      "-framework Cocoa"
      "-framework Carbon"
      "-framework ForceFeedback"
    )
  endif()
endif()

target_link_libraries(chainblocks-core-static Boost::context Boost::filesystem)

target_link_libraries(chainblocks-core-static 
  spdlog magic_enum nameof linalg xxHash
  pdqsort utf8.h Taskflow stb nlohmann_json m3 ghc_filesystem 
)

if(NOT EMSCRIPTEN)
  target_sources(chainblocks-core-static PRIVATE ${os_SOURCES})
else()
  target_sources(chainblocks-core-static PRIVATE coro.cpp) # emscripten coroutines
endif()

if(WIN32)
  target_link_libraries(chainblocks-core-static ssl crypto ws2_32 mswsock Secur32 Crypt32 NCrypt)
elseif(APPLE)
  if(NOT IOS)
    if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
      # these have to be full path or cmake will revert to use -l resulting in dynamic linking
      target_link_libraries(chainblocks-core-static
        /usr/local/opt/openssl@1.1/lib/libssl.a
        /usr/local/opt/openssl@1.1/lib/libcrypto.a
      )
    else()
      target_link_libraries(chainblocks-core-static
        -lssl -lcrypto
      )
    endif()
  endif()
  
  target_include_directories(chainblocks-core-static PUBLIC 
    /usr/local/include
    /usr/local/opt/openssl@1.1/include
  )
  target_link_directories(chainblocks-core-static PUBLIC
    /usr/local/lib
    /usr/local/opt/openssl@1.1/lib
  )
elseif(UNIX AND NOT EMSCRIPTEN)
  target_link_libraries(chainblocks-core-static ssl crypto pthread dl rt)
endif()

target_compile_options(chainblocks-core-static 
  PRIVATE -ffast-math -fno-finite-math-only -funroll-loops
)

target_compile_options(chainblocks-core-static 
  PUBLIC -Wno-multichar
  PRIVATE -Wno-missing-field-initializers -Wno-deprecated-copy
)

# TODO: Modularize block registration to remove Core=>Rust dependency
if(CHAINBLOCKS_WITH_EXTRA_BLOCKS)
  target_compile_definitions(chainblocks-core-static PUBLIC "-DCHAINBLOCKS_WITH_EXTRA_BLOCKS=1")
  target_link_libraries(chainblocks-core-static chainblocks-extra)
endif()

if(EMSCRIPTEN)
  em_link_js_library(chainblocks-core-static blocks/core.js)
  
  target_compile_options(chainblocks-core-static PUBLIC 
    "SHELL:-s DISABLE_EXCEPTION_CATCHING=0"
    "SHELL:-s FETCH=1"
    "SHELL:-s MIN_WEBGL_VERSION=2"
    "SHELL:-s MAX_WEBGL_VERSION=2"
  )
  
  target_link_options(chainblocks-core-static PUBLIC
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
  target_compile_definitions(chainblocks-core-static PUBLIC CHAINBLOCKS_NO_HTTP_BLOCKS=1)
endif()

duplicate_library_target(chainblocks-core-static SHARED chainblocks-core-shared)
target_compile_definitions(chainblocks-core-shared PUBLIC CHAINBLOCKS_CORE_DLL=1 chainblocks_core_EXPORTS=1)

add_library(chainblocks-core ALIAS chainblocks-core-static)

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