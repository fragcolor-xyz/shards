set(SOURCES
  flow.cpp
  linalg.cpp
  seqs.cpp
  strings.cpp
  wires.cpp
  casting.cpp
  logging.cpp
  serialization.cpp
  time.cpp
  exposed.cpp
  trait.cpp
  memoize.cpp
  parallel.cpp
)

if(ENABLE_FFI)
  list(APPEND CORE_RUST_FEATURES "ffi")
else()
  list(APPEND CORE_RUST_FEATURES "default")
endif()

# Manually add shards core source files as dependencies
set(CORE_PATH0 ../../rust)
set(CORE_PATH1 ../../rust_macro)
file(GLOB_RECURSE CORE_DEPS ${CORE_PATH0}/*.rs ${CORE_PATH0}/Cargo.toml ${CORE_PATH1}/*.rs ${CORE_PATH1}/Cargo.toml)
file(GLOB_RECURSE CORE_DEPS_REMOVE ${CORE_PATH0}/target/*.rs ${CORE_PATH1}/target/*.rs)
message(DEBUG ${CORE_DEPS_REMOVE})
list(REMOVE_ITEM CORE_DEPS_REMOVE ${CORE_DEPS_REMOVE})

# Add shards.h since lots of bindings depend on it
list(APPEND CORE_DEPS ${SHARDS_DIR}/include/shards/shards.h)

add_rust_library(NAME shards-core
  PROJECT_PATH ${CMAKE_CURRENT_LIST_DIR}
  FEATURES ${CORE_RUST_FEATURES}
  DEPENDS ${CORE_DEPS}
)

add_shards_module(core SOURCES ${SOURCES}
  REGISTER_SHARDS
  core casting flow linalg math seqs
  strings wires logging serialization time
  rust memoize exposed trait parallel
  RUST_TARGETS shards-core-rust
  INLINE_SOURCES core.cpp math.cpp inlined.cpp
  INLINE_SHARDS
  NotInline
  NoopShard
  CoreConst
  CoreInput
  CoreRepeat
  CoreGet
  CoreGetTableFixed
  CoreTakeTableFixed
  CoreRefRegular
  CoreRefTable
  CoreSetUpdateRegular
  CoreSetUpdateTable
  CorePush
  CoreIs
  CoreIsNot
  CoreIsTrue
  CoreIsFalse
  CoreIsNone
  CoreIsNotNone
  CoreAnd
  CoreOr
  CoreNot
  CoreIsMore
  CoreIsLess
  CoreIsMoreEqual
  CoreIsLessEqual
  MathAddInt64x2
  MathAddInt32x4
  MathAddFloat64x2
  MathAddFloat32x4
  MathSubtractInt64x2
  MathSubtractInt32x4
  MathSubtractFloat64x2
  MathSubtractFloat32x4
  MathMultiplyInt64x2
  MathMultiplyInt32x4
  MathMultiplyFloat64x2
  MathMultiplyFloat32x4
  MathXorInt64x2
  MathXorInt32x4
  MathAndInt64x2
  MathAndInt32x4
  MathOrInt64x2
  MathOrInt32x4
)

if(EMSCRIPTEN)
  em_link_js_library(shards-module-core core.js)
endif()

if(ENABLE_FFI AND WIN32)
  target_link_libraries(shards-module-core Dbghelp)
endif()

# Link SLEEF for SIMD-optimized audio math operations (non-Apple, non-WASM only)
# Apple uses Accelerate framework, WASM uses scalar fallback
is_module_enabled(AUDIO_ENABLED audio)
if(AUDIO_ENABLED AND NOT APPLE AND NOT EMSCRIPTEN)
  target_link_libraries(shards-module-core sleef)
endif()

if(APPLE)
  add_library(shards-core-swift-impl core.swift ${SHARDS_DIR}/include/shards/shards.swift)
  target_include_directories(shards-core-swift-impl PRIVATE ${SHARDS_DIR}/include)
  set_target_properties(shards-core-swift-impl PROPERTIES Swift_MODULE_NAME shards_core_swift)

  # ObjC @try/@catch shim that shards.swift binds via @_silgen_name (SHRunCatchingNSException).
  # Built as its own C/ObjC static lib so every generator (Ninja, Xcode framework, iOS)
  # compiles it with clang, and surfaced through the same interface that carries the Swift
  # impl so it lands on the final link right after it (resolving shards.o's reference).
  add_library(shards-core-objc-shim STATIC ${SHARDS_DIR}/include/shards/SHObjCException.m)
  target_include_directories(shards-core-objc-shim PRIVATE ${SHARDS_DIR}/include)

  # Intermediate target some workaround for the xcode generator propagating some non-existent swift modules in combination with target duplication not working
  add_library(shards-core-swift INTERFACE)
  target_link_libraries(shards-core-swift INTERFACE $<TARGET_FILE:shards-core-swift-impl> shards-core-objc-shim)
  add_dependencies(shards-core-swift shards-core-swift-impl)

  target_link_libraries(shards-module-core shards-core-swift)
endif()
