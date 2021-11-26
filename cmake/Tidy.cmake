#### Files to format and tidy
set(
  MY_PROJECT_SOURCE_FILES
  ${MY_PROJECT_SOURCE_FILES}
  ${CHAINBLOCKS_DIR}/include/blockwrapper.hpp
  ${CHAINBLOCKS_DIR}/include/dllblock.hpp
  ${CHAINBLOCKS_DIR}/include/utility.hpp
  ${CHAINBLOCKS_DIR}/include/chainblocks.hpp
  ${CHAINBLOCKS_DIR}/include/chainblocks.h
  ${CHAINBLOCKS_DIR}/include/common_types.hpp
  ${CHAINBLOCKS_DIR}/include/ops.hpp
  ${CHAINBLOCKS_DIR}/include/linalg_shim.hpp
  ${CHAINBLOCKS_DIR}/include/chain_dsl.hpp
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
  ${CHAINBLOCKS_DIR}/src/core/blocks/genetic.hpp
  ${CHAINBLOCKS_DIR}/src/core/blocks/random.cpp
  ${CHAINBLOCKS_DIR}/src/core/blocks/imaging.cpp
  ${CHAINBLOCKS_DIR}/src/core/blocks/http.cpp
  ${CHAINBLOCKS_DIR}/src/extra/bgfx.cpp
  ${CHAINBLOCKS_DIR}/src/extra/audio.cpp
  ${CHAINBLOCKS_DIR}/src/extra/bgfx.shaderc.cpp
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
  ${CHAINBLOCKS_DIR}/src/core/blocks/strings.cpp
  ${CHAINBLOCKS_DIR}/src/core/edn/print.hpp
  ${CHAINBLOCKS_DIR}/src/core/edn/read.hpp
  ${CHAINBLOCKS_DIR}/src/core/edn/eval.hpp
  ${CHAINBLOCKS_DIR}/src/core/edn/eval.cpp
  ${CHAINBLOCKS_DIR}/src/extra/snappy.cpp
  ${CHAINBLOCKS_DIR}/src/extra/bgfx_tests.cpp
  ${CHAINBLOCKS_DIR}/src/extra/gltf_tests.cpp
  ${CHAINBLOCKS_DIR}/src/extra/brotli.cpp
  ${CHAINBLOCKS_DIR}/src/extra/xr.cpp
  ${CHAINBLOCKS_DIR}/src/extra/inputs.cpp
  ${CHAINBLOCKS_DIR}/src/extra/gltf.cpp
  ${CHAINBLOCKS_DIR}/src/core/blocks/ws.cpp
  ${CHAINBLOCKS_DIR}/src/core/blocks/bigint.cpp
  ${CHAINBLOCKS_DIR}/src/core/blocks/wasm.cpp
  ${CHAINBLOCKS_DIR}/src/core/blocks/edn.cpp
  ${CHAINBLOCKS_DIR}/src/core/blocks/reflection.cpp
  ${CHAINBLOCKS_DIR}/src/tests/test_runtime.cpp
  ${CHAINBLOCKS_DIR}/src/core/cbccstrings.hpp
  ${CHAINBLOCKS_DIR}/src/extra/dsp.cpp
  )

#### Header paths for tidy
set(
  MY_PROJECT_HEADER_PATHS
  ${MY_PROJECT_HEADER_PATHS}
  -I${CHAINBLOCKS_DIR}/include
  -I${CHAINBLOCKS_DIR}/src/core
  -I${CHAINBLOCKS_DIR}/deps/replxx/include
  -I${CHAINBLOCKS_DIR}/deps/spdlog/include
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
  -I${CHAINBLOCKS_DIR}/deps/miniaudio
  -I${CHAINBLOCKS_DIR}/deps/wasm3/source
  -I${CHAINBLOCKS_DIR}/deps/linalg
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