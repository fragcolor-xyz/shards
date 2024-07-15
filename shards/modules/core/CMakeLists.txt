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
)

if(ENABLE_FFI)
  list(APPEND CORE_RUST_FEATURES "ffi")
else()
  list(APPEND CORE_RUST_FEATURES "default")
endif()

add_rust_library(NAME shards-core
  PROJECT_PATH ${CMAKE_CURRENT_LIST_DIR}
  FEATURES ${CORE_RUST_FEATURES})

add_shards_module(core SOURCES ${SOURCES}
  REGISTER_SHARDS
    core casting flow linalg math seqs
    strings wires logging serialization time
    rust memoize exposed trait
  RUST_TARGETS shards-core-rust
  INLINE_SOURCES core.cpp math.cpp inlined.cpp memoize.cpp
  INLINE_SHARDS
    NotInline
    NoopShard
    CoreMemoize
    CoreConst
    CoreSleep
    CoreInput
    CoreForRange
    CoreRepeat
    CoreOnce
    CoreGet
    CoreRefRegular
    CoreRefTable
    CoreSetUpdateRegular
    CoreSetUpdateTable
    CoreSwap
    CorePush
    CoreIs
    CoreIsNot
    CoreIsTrue
    CoreIsFalse
    CoreIsNone
    CoreAnd
    CoreOr
    CoreNot
    CoreIsMore
    CoreIsLess
    CoreIsMoreEqual
    CoreIsLessEqual
    MathAdd
    MathSubtract
    MathMultiply
    MathDivide
    MathXor
    MathAnd
    MathOr
    MathMod
    MathLShift
    MathRShift
    MathAbs
    MathExp
    MathExp2
    MathExpm1
    MathLog
    MathLog10
    MathLog2
    MathLog1p
    MathSqrt
    MathFastSqrt
    MathFastInvSqrt
    MathCbrt
    MathSin
    MathCos
    MathTan
    MathAsin
    MathAcos
    MathAtan
    MathSinh
    MathCosh
    MathTanh
    MathAsinh
    MathAcosh
    MathAtanh
    MathErf
    MathErfc
    MathTGamma
    MathLGamma
    MathCeil
    MathFloor
    MathTrunc
    MathRound
)

if(EMSCRIPTEN)
  em_link_js_library(shards-module-core core.js)
endif()

if(ENABLE_FFI AND WIN32)
  target_link_libraries(shards-module-core Dbghelp)
endif()

if(APPLE)
  # Build swift sources
  add_library(shards_core_swift core.swift ${SHARDS_DIR}/include/shards/shards.swift)
  target_include_directories(shards_core_swift PRIVATE ${SHARDS_DIR}/include)
endif()