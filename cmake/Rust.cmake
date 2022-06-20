# Automatic rust target config
set(Rust_BUILD_SUBDIR_HAS_TARGET ON)

set(CMAKE_FIND_USE_SYSTEM_ENVIRONMENT_PATH ON)
find_program(CARGO_EXE NAMES "cargo" REQUIRED)

if(NOT Rust_CARGO_TARGET)
  if(ANDROID)
    if(ANDROID_ABI MATCHES "arm64-v8a")
      set(Rust_CARGO_TARGET aarch64-linux-android)
    else()
      message(FATAL_ERROR "Unsupported rust android ABI: ${ANDROID_ABI}")
    endif()
  elseif(EMSCRIPTEN)
    set(Rust_CARGO_TARGET wasm32-unknown-emscripten)
  elseif(APPLE)
    if(IOS)
      set(PLATFORM "ios")
      if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64" AND XCODE_SDK MATCHES ".*simulator$")
        string(APPEND PLATFORM "-sim")
      endif()
    else()
      set(PLATFORM "darwin")
    endif()
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64")
      set(Rust_CARGO_TARGET aarch64-apple-${PLATFORM})
    else()
      set(Rust_CARGO_TARGET x86_64-apple-${PLATFORM})
    endif()
  elseif(WIN32 AND CMAKE_SIZEOF_VOID_P EQUAL 4)
    set(Rust_CARGO_TARGET i686-pc-windows-gnu)
  elseif(WIN32)
    set(Rust_CARGO_TARGET x86_64-pc-windows-${WINDOWS_ABI})
  elseif(LINUX)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64")
      set(Rust_CARGO_TARGET x86_64-unknown-linux-gnu)
    endif()
  endif()

  if(NOT Rust_CARGO_TARGET)
    message(FATAL_ERROR "Unsupported rust target")
  endif()
endif()

set(Rust_LIB_PREFIX ${LIB_PREFIX})
set(Rust_LIB_SUFFIX ${LIB_SUFFIX})

message(STATUS "Rust_CARGO_TARGET = ${Rust_CARGO_TARGET}")

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(Rust_BUILD_SUBDIR_CONFIGURATION debug)
else()
  set(Rust_CARGO_FLAGS --release)
  set(Rust_BUILD_SUBDIR_CONFIGURATION release)
endif()

if(Rust_BUILD_SUBDIR_HAS_TARGET)
  set(Rust_BUILD_SUBDIR ${Rust_CARGO_TARGET}/${Rust_BUILD_SUBDIR_CONFIGURATION})
else()
  set(Rust_BUILD_SUBDIR ${Rust_BUILD_SUBDIR_CONFIGURATION})
endif()
message(STATUS "Rust_BUILD_SUBDIR = ${Rust_BUILD_SUBDIR}")

set(Rust_FLAGS ${Rust_FLAGS} -Ctarget-cpu=${ARCH})

if(RUST_USE_LTO)
  set(Rust_FLAGS ${Rust_FLAGS} -Clinker-plugin-lto -Clinker=clang -Clink-arg=-fuse-ld=lld)
endif()

if(EMSCRIPTEN_PTHREADS)
  list(APPEND Rust_FLAGS -Ctarget-feature=+atomics,+bulk-memory)
  list(APPEND Rust_CARGO_UNSTABLE_FLAGS -Zbuild-std=panic_abort,std)
  set(RUST_NIGHTLY TRUE)
endif()

# Currently required for --crate-type argument
set(RUST_NIGHTLY TRUE)
if(RUST_NIGHTLY)
  list(APPEND Rust_CARGO_UNSTABLE_FLAGS -Zunstable-options)
  set(Rust_CARGO_TOOLCHAIN "+nightly")
endif()

macro(ADD_RUST_FEATURE VAR FEATURE)
  if(${VAR})
    set(${VAR} ${${VAR}},${FEATURE})
  else()
    set(${VAR} ${FEATURE})
  endif()
endmacro()

# Defines a rust target
#   this creates a static library target named ${NAME}-rust
#   that you can link against
function(add_rust_library)
  set(OPTS)
  set(ARGS
    NAME                 # (Required) The name of the generated rust library target (and the generated static library name)
    PROJECT_PATH         # (Required) The path to the cargo project to build
    TARGET_PATH          # (Optional) Override the rust target path to use
    TARGET_NAME          # (Optional) Override name of the generated target
  )
  set(MULTI_ARGS
    FEATURES             # (Optional) List of features to pass to rust build
    ENVIRONMENT          # (Optional) Environment variables
    DEPENDS              # (Optional) Extra file-level dependencies
  )
  cmake_parse_arguments(RUST "${OPTS}" "${ARGS}" "${MULTI_ARGS}" ${ARGN})

  message(STATUS "add_rust_library(${RUST_PROJECT_PATH})")
  message(STATUS "  NAME = ${RUST_NAME}")
  message(STATUS "  PROJECT_PATH = ${RUST_PROJECT_PATH}")
  message(STATUS "  TARGET_PATH = ${RUST_TARGET_PATH}")
  message(STATUS "  TARGET_NAME = ${RUST_TARGET_NAME}")

  if(RUST_FEATURES)
    list(JOIN RUST_FEATURES "," RUST_FEATURES_STRING)
    set(RUST_FEATURES_ARG --features "${RUST_FEATURES_STRING}")
    message(STATUS "  Rust features: ${RUST_FEATURES_STRING}")
  endif()

  message(STATUS "  > Scanning rust sources in: ${RUST_PROJECT_PATH}")
  file(GLOB_RECURSE RUST_SOURCES "${RUST_PROJECT_PATH}/*.rs" "${RUST_PROJECT_PATH}/*.toml")

  file(GLOB_RECURSE RUST_TEMP_SOURCES "${RUST_PROJECT_PATH}/target/*.rs" "${RUST_PROJECT_PATH}/target/*.toml")
  list(REMOVE_ITEM RUST_SOURCES ${RUST_TEMP_SOURCES})
  message(STATUS "   Rust sources: ${RUST_SOURCES}")

  if(Rust_CARGO_TARGET)
    set(RUST_TARGET_ARG --target ${Rust_CARGO_TARGET})
  endif()

  if(NOT RUST_TARGET_NAME)
    set(RUST_TARGET_NAME "${RUST_NAME}-rust")
  endif()
  set(CUSTOM_TARGET_NAME "cargo-${RUST_TARGET_NAME}")
  message(STATUS "  Rust target name: ${RUST_TARGET_NAME}")

  if(NOT RUST_TARGET_PATH)
    set(RUST_TARGET_PATH ${RUST_PROJECT_PATH}/target)
  endif()
  message(STATUS "  Rust target path: ${RUST_TARGET_PATH}")

  # Derive lib name
  # - Replace - with _
  # - add libXYZ prefix
  # - add .a or .lib
  set(GENERATED_LIB_NAME ${Rust_LIB_PREFIX}${RUST_NAME}${Rust_LIB_SUFFIX})
  string(REPLACE "-" "_" GENERATED_LIB_NAME "${GENERATED_LIB_NAME}")
  set(GENERATED_LIB_PATH ${RUST_TARGET_PATH}/${Rust_BUILD_SUBDIR}/${GENERATED_LIB_NAME})
  message(STATUS "  Rust generated lib path: ${GENERATED_LIB_PATH}")

  set(RUST_CRATE_TYPE_ARG --crate-type staticlib)

  add_custom_command(
    OUTPUT ${GENERATED_LIB_PATH}
    COMMAND ${CMAKE_COMMAND} -E env RUSTFLAGS="${Rust_FLAGS}" ${ENVIRONMENT} ${BUILD_SCRIPT} ${CARGO_EXE} ${Rust_CARGO_TOOLCHAIN} rustc ${Rust_CARGO_UNSTABLE_FLAGS} ${RUST_FEATURES_ARG} ${RUST_CRATE_TYPE_ARG} ${RUST_TARGET_ARG} ${Rust_CARGO_FLAGS}
    WORKING_DIRECTORY ${RUST_PROJECT_PATH}
    DEPENDS ${RUST_SOURCES} ${RUST_DEPENDS}
    USES_TERMINAL
  )

  # The rust custom target
  add_custom_target(
    ${CUSTOM_TARGET_NAME}
    DEPENDS ${GENERATED_LIB_PATH}
  )

  # Library target to wrap around the custom build target
  add_library(${RUST_TARGET_NAME} STATIC IMPORTED GLOBAL)
  add_dependencies(${RUST_TARGET_NAME} ${CUSTOM_TARGET_NAME})
  set_target_properties(${RUST_TARGET_NAME} PROPERTIES
    IMPORTED_LOCATION ${GENERATED_LIB_PATH}
  )
endfunction()