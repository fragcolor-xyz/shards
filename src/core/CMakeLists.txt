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

add_library(chainblocks-core-base STATIC ${core_SOURCES})

target_include_directories(chainblocks-core-base PUBLIC ../../include)
target_include_directories(chainblocks-core-base PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

if(EMSCRIPTEN)
  target_compile_options(chainblocks-core-base PRIVATE "SHELL:-s DISABLE_EXCEPTION_CATCHING=0")
endif()

if(CHAINBLOCKS_NO_BIGINT_BLOCKS)
  target_compile_definitions(chainblocks-core-base PUBLIC CHAINBLOCKS_NO_BIGINT_BLOCKS=1)
else()
  target_sources(chainblocks-core-base PRIVATE blocks/bigint.cpp)
endif()

if(APPLE)
  target_link_libraries(chainblocks-core-base
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
    target_link_libraries(chainblocks-core-base
      "-framework hidapi"
      "-framework AVFoundation"
      "-framework GameController"
      "-framework CoreMotion"
      "-framework CoreGraphics"
      "-framework CoreHaptics"
      "-framework UIKit"
    )
  else()
    target_link_libraries(chainblocks-core-base
      "-framework Cocoa"
      "-framework Carbon"
      "-framework ForceFeedback"
    )
  endif()
endif()

if(IOS)
  unset(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE)
  find_package(Boost REQUIRED)

  # FindBoost doesn't work on iOS
  # Compile-in boost-context implementation
  enable_language(ASM)
  if(X86 OR X86_IOS_SIMULATOR)
    set(BOOST_CONTEXT_SOURCES
      ${CHAINBLOCKS_DIR}/deps/boost-context/src/posix/stack_traits.cpp
      ${CHAINBLOCKS_DIR}/deps/boost-context/src/asm/jump_x86_64_sysv_macho_gas.S
      ${CHAINBLOCKS_DIR}/deps/boost-context/src/asm/make_x86_64_sysv_macho_gas.S
      ${CHAINBLOCKS_DIR}/deps/boost-context/src/asm/ontop_x86_64_sysv_macho_gas.S
    )
  else()
    set(BOOST_CONTEXT_SOURCES
      ${CHAINBLOCKS_DIR}/deps/boost-context/src/posix/stack_traits.cpp
      ${CHAINBLOCKS_DIR}/deps/boost-context/src/asm/jump_arm64_aapcs_macho_gas.S
      ${CHAINBLOCKS_DIR}/deps/boost-context/src/asm/make_arm64_aapcs_macho_gas.S
      ${CHAINBLOCKS_DIR}/deps/boost-context/src/asm/ontop_arm64_aapcs_macho_gas.S
    )
  endif()
  add_library(boost-context STATIC ${BOOST_CONTEXT_SOURCES})
  target_include_directories(boost-context PUBLIC ${CHAINBLOCKS_DIR}/deps/boost-context/include ${Boost_INCLUDE_DIRS})
  target_link_libraries(chainblocks-core-base boost-context)
elseif(EMSCRIPTEN)
  # emcmake sets this, but can't find boost anymore, see https://github.com/emscripten-core/emscripten/issues/10078
  unset(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE)

  # Only need the headers
  find_package(Boost REQUIRED)
  
  target_include_directories(chainblocks-core-base PUBLIC ${Boost_INCLUDE_DIRS})
else()
  if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(Boost_USE_STATIC_LIBS ON)
  endif()
  
  find_package(Boost REQUIRED COMPONENTS context filesystem)
  target_link_libraries(chainblocks-core-base Boost::context Boost::filesystem)
endif()

target_link_libraries(chainblocks-core-base 
  spdlog magic_enum nameof linalg xxHash
  pdqsort utf8.h Taskflow stb nlohmann_json m3 ghc_filesystem 
)

if(NOT EMSCRIPTEN)
  target_sources(chainblocks-core-base PRIVATE ${os_SOURCES})
else()
  target_sources(chainblocks-core-base PRIVATE coro.cpp) # emscripten coroutines
endif()

if(WIN32)
  target_link_libraries(chainblocks-core-base ssl crypto ws2_32 mswsock Secur32 Crypt32 NCrypt)
elseif(APPLE)
  if(NOT IOS)
    if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
      # these have to be full path or cmake will revert to use -l resulting in dynamic linking
      target_link_libraries(chainblocks-core-base
        /usr/local/opt/openssl@1.1/lib/libssl.a
        /usr/local/opt/openssl@1.1/lib/libcrypto.a
      )
    else()
      target_link_libraries(chainblocks-core-base
        -lssl -lcrypto
      )
    endif()
  endif()
  
  target_include_directories(chainblocks-core-base PUBLIC 
    /usr/local/include
    /usr/local/opt/openssl@1.1/include
  )
  target_link_directories(chainblocks-core-base PUBLIC
    /usr/local/lib
    /usr/local/opt/openssl@1.1/lib
  )
elseif(UNIX AND NOT EMSCRIPTEN)
  target_link_libraries(chainblocks-core-base ssl crypto pthread dl rt)
endif()

target_compile_options(chainblocks-core-base 
  PRIVATE -ffast-math -fno-finite-math-only -funroll-loops
)

target_compile_options(chainblocks-core-base 
  PUBLIC -Wno-multichar
  PRIVATE -Wno-missing-field-initializers -Wno-deprecated-copy
)

# TODO: Modularize block registration to remove Core=>Rust dependency
if(CHAINBLOCKS_WITH_EXTRA_BLOCKS)
  target_compile_definitions(chainblocks-core-base PUBLIC "-DCHAINBLOCKS_WITH_EXTRA_BLOCKS=1")
  target_link_libraries(chainblocks-core-base chainblocks-extra)
endif()

if(EMSCRIPTEN)
  em_link_js_library(chainblocks-core-base blocks/core.js)
  
  target_compile_options(chainblocks-core-base PUBLIC 
    "SHELL:-s DISABLE_EXCEPTION_CATCHING=0"
    "SHELL:-s FETCH=1"
    "SHELL:-s MIN_WEBGL_VERSION=2"
    "SHELL:-s MAX_WEBGL_VERSION=2"
  )
  
  target_link_options(chainblocks-core-base PUBLIC
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
  target_compile_definitions(chainblocks-core-base PUBLIC CHAINBLOCKS_NO_HTTP_BLOCKS=1)
endif()

add_library(chainblocks-core-shared SHARED runtime.cpp)
target_link_libraries(chainblocks-core-shared PUBLIC chainblocks-core-base)
target_compile_definitions(chainblocks-core-shared PUBLIC CHAINBLOCKS_CORE_DLL=1 chainblocks_core_EXPORTS=1)

add_library(chainblocks-core-static runtime.cpp)
target_link_libraries(chainblocks-core-static PUBLIC chainblocks-core-base)

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