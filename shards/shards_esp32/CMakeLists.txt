# ESP-IDF Component CMakeLists.txt for Shards
# This file is used when building Shards as an ESP-IDF component.
#
# NOTE (esp32-p4 revival): this is an incremental bring-up. It currently builds
# only the glue translation units to flush out the header-level compile tail for
# rv32 + IDF. Core sources get wired in once the headers compile clean.

get_filename_component(SHARDS_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)

# IDF expands each component CMakeLists twice, once in `cmake -P` SCRIPT MODE to
# scan REQUIRES. CPM's define_property() is not scriptable, so CPM cannot run here.
# Reuse the already-downloaded header-only deps from the desktop CPM cache instead.
set(CPM_CACHE "$ENV{CPM_SOURCE_CACHE}")
if(NOT CPM_CACHE)
  set(CPM_CACHE "$ENV{HOME}/.cache/CPM")
endif()
# Anchor on a marker file so we land on the versioned source dir, not the sibling
# cmake.lock file that also lives under each cache entry.
file(GLOB _entt_marker "${CPM_CACHE}/entt/*/src/entt/entt.hpp")
file(GLOB _linalg_marker "${CPM_CACHE}/linalg/*/linalg.h")
file(GLOB _stb_marker "${CPM_CACHE}/stb/*/stb_image.h")
file(GLOB _tf_marker "${CPM_CACHE}/cpp-taskflow/*/taskflow/taskflow.hpp")
file(GLOB _utf8_marker "${CPM_CACHE}/utf8.h/*/utf8.h")
list(GET _entt_marker 0 _entt_m)
# marker is <ver>/src/entt/entt.hpp; entt_SOURCE_DIR must be <ver> so /src appends
get_filename_component(entt_SOURCE_DIR "${_entt_m}/../../.." ABSOLUTE)
list(GET _linalg_marker 0 _linalg_m)
get_filename_component(linalg_SOURCE_DIR "${_linalg_m}" DIRECTORY)
list(GET _stb_marker 0 _stb_m)
get_filename_component(stb_SOURCE_DIR "${_stb_m}" DIRECTORY)
list(GET _tf_marker 0 _tf_m)
get_filename_component(taskflow_SOURCE_DIR "${_tf_m}/../.." ABSOLUTE)
list(GET _utf8_marker 0 _utf8_m)
get_filename_component(utf8_SOURCE_DIR "${_utf8_m}" DIRECTORY)

# gfx/linalg.hpp includes <linalg/linalg.h>; the cache dir holds linalg.h at its
# root, so make a shim dir with a `linalg` symlink so that include resolves.
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/sh_shim)
if(NOT EXISTS ${CMAKE_CURRENT_BINARY_DIR}/sh_shim/linalg)
  file(CREATE_LINK ${linalg_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}/sh_shim/linalg SYMBOLIC)
endif()

# Boost is CPM-downloaded by the normal desktop build into build/<cfg>/_deps/boost-src.
# Reuse those modular headers here (header-only usage: align, atomic, container, ...).
set(BOOST_SRC "" CACHE PATH "Path to a boost source tree (modular layout with libs/*/include)")
if(NOT BOOST_SRC)
  foreach(cfg Release Debug)
    if(EXISTS "${SHARDS_ROOT}/build/${cfg}/_deps/boost-src/libs")
      set(BOOST_SRC "${SHARDS_ROOT}/build/${cfg}/_deps/boost-src")
      break()
    endif()
  endforeach()
endif()
if(NOT BOOST_SRC OR NOT EXISTS "${BOOST_SRC}/libs")
  message(FATAL_ERROR "shards_esp32: could not locate boost sources. Set -DBOOST_SRC=<path to modular boost>")
endif()
file(GLOB BOOST_LIB_INCLUDES "${BOOST_SRC}/libs/*/include")
message(STATUS "shards_esp32: using boost headers from ${BOOST_SRC} (${BOOST_LIB_INCLUDES})")

# Glue (coro_freertos.cpp dropped: rv32 uses the desktop custom-fcontext fiber).
set(ESP32_SOURCES
    src/fs_esp32.cpp
    src/main_esp32.cpp
)

# Core runtime sources + rv32 fcontext assembly + fast_string. core_inlined.cpp
# pulls runtime.cpp (registerShards/GetGlobals/...). SHARDS_INLINE_EVERYTHING=0 so
# the inlined shard activations come from modules/core separately if needed.
set(SHARDS_CORE_CPP
    ${SHARDS_ROOT}/shards/core/core_inlined.cpp
    ${SHARDS_ROOT}/shards/core/coro.cpp
    ${SHARDS_ROOT}/shards/core/ops_internal.cpp
    ${SHARDS_ROOT}/shards/core/number_types.cpp
    ${SHARDS_ROOT}/shards/core/utils.cpp
    ${SHARDS_ROOT}/shards/core/wire_dsl.cpp
    ${SHARDS_ROOT}/shards/core/trait.cpp
    ${SHARDS_ROOT}/shards/core/type_info.cpp
    ${SHARDS_ROOT}/shards/core/hash.cpp
    ${SHARDS_ROOT}/shards/core/log_api.cpp
    ${SHARDS_ROOT}/shards/core/wires.cpp
    ${SHARDS_ROOT}/shards/core/async.cpp
    ${SHARDS_ROOT}/shards/core/pmr/shared_temp_allocator.cpp
    ${SHARDS_ROOT}/shards/log/log.cpp
    ${SHARDS_ROOT}/shards/fast_string/storage.cpp
)
set(SHARDS_CORE_ASM ${SHARDS_ROOT}/shards/core/asm/fcontext_riscv_elf.S)

# Rust-FREE subset of the core module's C++ shards (Const/Log/Math/If/cast/seqs/...).
# Compiled with SHARDS_THIS_MODULE_ID=core so SHARDS_REGISTER_FN expands to
# shardsRegister_core_<id> (see esp32_stubs.cpp registerModuleShards). Excludes
# rust/serialization/memoize/parallel/wires.
set(SHARDS_MODULE_CORE_CPP
    ${SHARDS_ROOT}/shards/modules/core/core.cpp
    ${SHARDS_ROOT}/shards/modules/core/casting.cpp
    ${SHARDS_ROOT}/shards/modules/core/flow.cpp
    ${SHARDS_ROOT}/shards/modules/core/linalg.cpp
    ${SHARDS_ROOT}/shards/modules/core/math.cpp
    ${SHARDS_ROOT}/shards/modules/core/seqs.cpp
    ${SHARDS_ROOT}/shards/modules/core/strings.cpp
    ${SHARDS_ROOT}/shards/modules/core/logging.cpp
    ${SHARDS_ROOT}/shards/modules/core/time.cpp
    ${SHARDS_ROOT}/shards/modules/core/exposed.cpp
    ${SHARDS_ROOT}/shards/modules/core/trait.cpp
)

# boost.container pmr resources (fast_string non-TBB path uses monotonic_buffer_resource).
# Compiled WITHOUT the shards prelude (they're upstream boost, not shards code).
set(BOOST_CONTAINER_SRC
    ${BOOST_SRC}/libs/container/src/global_resource.cpp
    ${BOOST_SRC}/libs/container/src/monotonic_buffer_resource.cpp
)

list(APPEND ESP32_SOURCES ${SHARDS_CORE_CPP} ${SHARDS_MODULE_CORE_CPP} ${SHARDS_CORE_ASM} ${BOOST_CONTAINER_SRC} src/esp32_stubs.cpp)

idf_component_register(
    SRCS ${ESP32_SOURCES}
    INCLUDE_DIRS
        include
        ${SHARDS_ROOT}
        ${SHARDS_ROOT}/include
        ${SHARDS_ROOT}/shards
        ${SHARDS_ROOT}/shards/core
        ${SHARDS_ROOT}/deps/spdlog/include
        ${SHARDS_ROOT}/deps/magic_enum/include
        ${SHARDS_ROOT}/deps/nameof/include
        ${SHARDS_ROOT}/deps/xxHash
        ${SHARDS_ROOT}/deps/json/include
        ${SHARDS_ROOT}/deps/tracy_stub
        ${taskflow_SOURCE_DIR}
        # Generated InlineShard enum (shards/inlined.hpp) from the desktop build.
        ${SHARDS_ROOT}/build/Release/src/union/generated
        ${BOOST_LIB_INCLUDES}
        ${linalg_SOURCE_DIR}
        ${CMAKE_CURRENT_BINARY_DIR}/sh_shim
        ${utf8_SOURCE_DIR}
        ${entt_SOURCE_DIR}/src
        ${stb_SOURCE_DIR}
    REQUIRES freertos vfs
    PRIV_REQUIRES log spiffs
)

# Replace the desktop PCH (pch.cpp): force-include the esp32 prelude into every
# core C++ TU so they get <vector>/<mutex>/Tracy/SHLOG_*/runtime.hpp like the PCH
# guarantees. NOT applied to the .S asm.
set_source_files_properties(
    ${SHARDS_CORE_CPP} ${SHARDS_MODULE_CORE_CPP}
    PROPERTIES COMPILE_OPTIONS "-include;${CMAKE_CURRENT_LIST_DIR}/include/sh_esp32_prelude.hpp"
)
# The core module's shards register as shardsRegister_core_<id>.
set_source_files_properties(
    ${SHARDS_MODULE_CORE_CPP}
    PROPERTIES COMPILE_DEFINITIONS "SHARDS_THIS_MODULE_ID=core"
)

target_compile_definitions(${COMPONENT_LIB} PUBLIC
    SH_ESP32=1
    SH_FREERTOS=1
    SH_STRIP_HELP_STRINGS=1    # drop per-shard help registration (~366 KB static-init + rodata)
    SHARDS_WITH_ENTT=1         # entt is gated behind this in shards.hpp
    SH_CUSTOM_FCONTEXT=1        # rv32 fcontext fiber (asm/fcontext_riscv_elf.S)
    FAST_STRING_NO_TBB=1        # boost.container fallback instead of TBB
    HAS_ASYNC_SUPPORT=0
    SH_ENABLE_TIDE_POOL=0
    HAS_BOOST_CONTAINER=1
    SHARDS_INLINE_EVERYTHING=0
    SHARDS_DEBUGGER=0
    SPDLOG_NO_EXCEPTIONS=1
    SPDLOG_NO_THREAD_ID=1
    SHARDS_USE_ESP_LOG=1
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_20)

# Exceptions + RTTI stay ON for phase-1 bring-up (IDF supports them; removal is a
# later size optimization, not a prerequisite). See sdkconfig.defaults.
target_compile_options(${COMPONENT_LIB} PRIVATE
    -Wno-unused-parameter
    -Wno-missing-field-initializers
    -Wno-error=maybe-uninitialized   # false positive in vendored xxhash.h on gcc 14
    # Pre-existing warnings in core module headers that IDF promotes to errors.
    -Wno-error=parentheses
    -Wno-error=class-memaccess
    # thread_local (shards::logging::threadState) defaults to global-dynamic TLS ->
    # GOT/__tls_get_addr -> .got.plt, which ESP32's static link script discards.
    # A static firmware is always local-exec.
    -ftls-model=local-exec
)

if(CONFIG_SHARDS_STACK_SIZE)
    target_compile_definitions(${COMPONENT_LIB} PUBLIC CONFIG_SHARDS_STACK_SIZE=${CONFIG_SHARDS_STACK_SIZE})
endif()
