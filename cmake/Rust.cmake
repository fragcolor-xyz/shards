# Automatic rust target config
set(RUST_BUILD_SUBDIR_HAS_TARGET ON)

set(CMAKE_FIND_USE_SYSTEM_ENVIRONMENT_PATH ON)
find_program(CARGO_EXE NAMES "cargo" REQUIRED)

# set(CARGO_EXE ${CMAKE_CURRENT_LIST_DIR}/test_script.sh)
if(NOT RUST_CARGO_TARGET)
  if(ANDROID)
    if(ANDROID_ABI MATCHES "arm64-v8a")
      set(RUST_CARGO_TARGET aarch64-linux-android)
    else()
      message(FATAL_ERROR "Unsupported rust android ABI: ${ANDROID_ABI}")
    endif()
  elseif(EMSCRIPTEN)
    set(RUST_CARGO_TARGET wasm32-unknown-emscripten)
  elseif(APPLE)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64")
      list(APPEND RUST_FLAGS -Ctarget-feature=+fp16,+fhm)
    endif()

    if(VISIONOS)
      set(PLATFORM "visionos")

      if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64" AND XCODE_SDK MATCHES ".*simulator$")
        string(APPEND PLATFORM "-sim")
      endif()

      list(APPEND RUST_CARGO_UNSTABLE_FLAGS -Zbuild-std)
      set(RUST_NIGHTLY TRUE)
    elseif(IOS)
      set(PLATFORM "ios")

      if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64" AND XCODE_SDK MATCHES ".*simulator$")
        string(APPEND PLATFORM "-sim")
      endif()
    elseif(WATCHOS)
      set(PLATFORM "watchos")

      if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64" AND XCODE_SDK MATCHES ".*simulator$")
        string(APPEND PLATFORM "-sim")
      endif()

      list(APPEND RUST_CARGO_UNSTABLE_FLAGS -Zbuild-std)
      set(RUST_NIGHTLY TRUE)
    else()
      set(PLATFORM "darwin")
    endif()

    if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64")
      set(RUST_CARGO_TARGET aarch64-apple-${PLATFORM})
    else()
      set(RUST_CARGO_TARGET x86_64-apple-${PLATFORM})
    endif()
  elseif(WIN32 AND CMAKE_SIZEOF_VOID_P EQUAL 4)
    set(RUST_CARGO_TARGET i686-pc-windows-gnu)
  elseif(WIN32)
    set(RUST_CARGO_TARGET x86_64-pc-windows-${WINDOWS_ABI})
  elseif(ZIG_MUSL)
    # Zig musl cross-compilation
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
      set(RUST_CARGO_TARGET aarch64-unknown-linux-musl)
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64")
      set(RUST_CARGO_TARGET x86_64-unknown-linux-musl)
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "riscv64")
      # riscv64gc-unknown-linux-musl doesn't have pre-built std, needs -Zbuild-std
      set(RUST_CARGO_TARGET riscv64gc-unknown-linux-musl)
      list(APPEND RUST_CARGO_UNSTABLE_FLAGS -Zbuild-std)
      set(RUST_NIGHTLY TRUE)
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "riscv32")
      # Tier 3 target - requires -Z build-std
      set(RUST_CARGO_TARGET riscv32gc-unknown-linux-musl)
      set(RUST_RISCV32 TRUE)
      list(APPEND RUST_CARGO_UNSTABLE_FLAGS -Zbuild-std)
      set(RUST_NIGHTLY TRUE)
    else()
      message(FATAL_ERROR "Unsupported Zig musl architecture: ${CMAKE_SYSTEM_PROCESSOR}")
    endif()
    # Static CRT for musl
    list(APPEND RUST_FLAGS -Ctarget-feature=+crt-static)
    message(STATUS "Zig musl Rust target: ${RUST_CARGO_TARGET}")
  elseif(DESKTOP_LINUX)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64")
      set(RUST_CARGO_TARGET x86_64-unknown-linux-gnu)
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
      set(RUST_CARGO_TARGET aarch64-unknown-linux-gnu)
    endif()
  endif()

  if(NOT RUST_CARGO_TARGET)
    message(FATAL_ERROR "Unsupported rust target")
  endif()
endif()

set(RUST_LIB_PREFIX ${LIB_PREFIX})
set(RUST_LIB_SUFFIX ${LIB_SUFFIX})

message(STATUS "RUST_CARGO_TARGET = ${RUST_CARGO_TARGET}")

set(RUST_CARGO_FLAGS "" CACHE STRING "Flags added to rust builds")
set(RUST_CARGO_FLAGS_INT ${RUST_CARGO_FLAGS})

# Set RUST_BUILD_TYPE to override the rust build type
if(NOT RUST_BUILD_TYPE)
  set(RUST_BUILD_TYPE ${CMAKE_BUILD_TYPE})
endif()

if(RUST_BUILD_TYPE STREQUAL "Debug")
  set(RUST_BUILD_SUBDIR_CONFIGURATION debug)
elseif(RUST_BUILD_TYPE STREQUAL "RelWithDebInfo")
  set(RUST_CARGO_FLAGS_INT --profile rel-with-deb-info)
  set(RUST_BUILD_SUBDIR_CONFIGURATION rel-with-deb-info)
elseif(RUST_BUILD_TYPE STREQUAL "Small")
  set(RUST_CARGO_FLAGS_INT --profile small)
  set(RUST_BUILD_SUBDIR_CONFIGURATION small)
  list(APPEND RUST_FLAGS -Zlocation-detail=none)
  list(APPEND RUSTC_FLAGS
    -Zbuild-std=std
    -Zbuild-std-features=optimize_for_size
  )
elseif(RUST_BUILD_TYPE STREQUAL "ExtraSmall")
  set(RUST_CARGO_FLAGS_INT --profile extra-small)
  set(RUST_BUILD_SUBDIR_CONFIGURATION extra-small)
  list(APPEND RUST_FLAGS -Zlocation-detail=none -Zunstable-options -Cpanic=immediate-abort)
  list(APPEND RUSTC_FLAGS
    -Zbuild-std=std,panic_abort
    -Zbuild-std-features=optimize_for_size
  )
else()
  set(RUST_CARGO_FLAGS_INT --release)
  set(RUST_BUILD_SUBDIR_CONFIGURATION release)
endif()

if(TRACY_ENABLE)
  set(RUST_BUILD_SUBDIR_CONFIGURATION ${RUST_BUILD_SUBDIR_CONFIGURATION}-tracy)
  set(RUST_CARGO_FLAGS_INT --profile ${RUST_BUILD_SUBDIR_CONFIGURATION})
endif()

if(RUST_BUILD_SUBDIR_HAS_TARGET)
  set(RUST_BUILD_SUBDIR ${RUST_CARGO_TARGET}/${RUST_BUILD_SUBDIR_CONFIGURATION})
else()
  set(RUST_BUILD_SUBDIR ${RUST_BUILD_SUBDIR_CONFIGURATION})
endif()

message(STATUS "RUST_BUILD_SUBDIR = ${RUST_BUILD_SUBDIR}")

if(ARCH)
  list(APPEND RUST_FLAGS -Ctarget-cpu=${ARCH})
endif()

if(RUST_USE_LTO)
  list(APPEND RUST_FLAGS -Clinker-plugin-lto -Clinker=clang -Clink-arg=-fuse-ld=lld)
endif()

if(EMSCRIPTEN_PTHREADS)
  list(APPEND RUST_FLAGS -Ctarget-feature=+atomics,+bulk-memory)
  list(APPEND RUST_CARGO_UNSTABLE_FLAGS -Zbuild-std)
  set(RUST_NIGHTLY TRUE)
endif()

if(WIN32)
  if(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    list(APPEND RUST_FLAGS -Ctarget-feature=+crt-static)
  endif()
endif()

if(USE_RUST_TSAN)
  list(APPEND RUST_FLAGS -Zsanitizer=thread)
endif()

if(CODE_COVERAGE)
  list(APPEND RUST_FLAGS -Cinstrument-coverage)
endif()

option(CARGO_OFFLINE_MODE "Use offline mode for cargo" OFF)

if(CARGO_OFFLINE_MODE)
  list(APPEND RUSTC_FLAGS --offline)
endif()

if(CARGO_VERBOSE)
  list(APPEND RUSTC_FLAGS -vv)
endif()

# if(USE_ASAN)
# list(APPEND RUST_FLAGS -Zsanitizer=address)
# endif()

# Currently required for --crate-type argument
list(APPEND RUST_CARGO_UNSTABLE_FLAGS -Zunstable-options)

file(READ ${SHARDS_DIR}/rust.version RUST_TOOLCHAIN_DEFAULT)

set(RUST_TOOLCHAIN "" CACHE STRING "Override the rust toolchain to use")

if(NOT(RUST_TOOLCHAIN STREQUAL ""))
  # Use cmake option if set
  set(RUST_TOOLCHAIN_OVERRIDE "+${RUST_TOOLCHAIN}")
elseif(NOT(RUST_TOOLCHAIN_DEFAULT STREQUAL ""))
  # Use environment variable if set
  set(RUST_TOOLCHAIN_OVERRIDE "+${RUST_TOOLCHAIN_DEFAULT}")
else()
  unset(RUST_TOOLCHAIN_OVERRIDE)
endif()

if(RUST_TOOLCHAIN_OVERRIDE)
  message(STATUS "Using rust toolchain: ${RUST_TOOLCHAIN_OVERRIDE}")
endif()

macro(ADD_RUST_FEATURE VAR FEATURE)
  if(${VAR})
    set(${VAR} ${${VAR}},${FEATURE})
  else()
    set(${VAR} ${FEATURE})
  endif()
endmacro()

# Need this custom build script to inherit the correct SDK variables from XCode
if(IOS OR VISIONOS OR WATCHOS)
  set(RUST_BUILD_SCRIPT "${CMAKE_CURRENT_LIST_DIR}/osx_rust_build.sh" ${XCODE_SDK})
endif()

# Defines a rust target
# this creates a static library target named ${NAME}-rust
# that you can link against
function(add_rust_library)
  set(OPTS)
  set(ARGS
    NAME # (Required) The name of the rust package to build (and the generated static library name)
    PROJECT_PATH # (Required) The path to the cargo project to build
    TARGET_PATH # (Optional) Override the rust target path to use
    TARGET_NAME # (Optional) Override name of the generated target
  )
  set(MULTI_ARGS
    FEATURES # (Optional) List of features to pass to rust build
    ENVIRONMENT # (Optional) Environment variables
    DEPENDS # (Optional) Extra file-level dependencies
    EXCLUDE_DEPENDS # (Optional) Extra file-level dependencies to ignore
    OUTPUTS # (Optional) Addtional outputs (e.g. bindings) generated by the target
  )
  cmake_parse_arguments(RUST "${OPTS}" "${ARGS}" "${MULTI_ARGS}" ${ARGN})

  message(STATUS "add_rust_library(${RUST_PROJECT_PATH})")
  message(VERBOSE "  NAME = ${RUST_NAME}")
  message(VERBOSE "  PROJECT_PATH = ${RUST_PROJECT_PATH}")
  message(VERBOSE "  TARGET_PATH = ${RUST_TARGET_PATH}")
  message(VERBOSE "  TARGET_NAME = ${RUST_TARGET_NAME}")

  if(NOT RUST_NAME)
    message(FATAL_ERROR "NAME <name> is required")
  endif()

  if(NOT RUST_NAME)
    message(FATAL_ERROR "PROJECT_PATH <path> is required")
  endif()

  if(RUST_FEATURES)
    list(JOIN RUST_FEATURES "," RUST_FEATURES_STRING)
    set(RUST_FEATURES_ARG --features "${RUST_FEATURES_STRING}")
    message(STATUS "  RUST_FEATURES_STRING: ${RUST_FEATURES_STRING}")
  endif()

  file(GLOB_RECURSE RUST_SOURCES "${RUST_PROJECT_PATH}/*.rs" "${RUST_PROJECT_PATH}/Cargo.toml" "${RUST_PROJECT_PATH}/Cargo.lock")
  file(GLOB_RECURSE RUST_TEMP_SOURCES "${RUST_PROJECT_PATH}/target/*.rs" "${RUST_PROJECT_PATH}/target/*.toml")

  if(RUST_TEMP_SOURCES)
    list(REMOVE_ITEM RUST_SOURCES ${RUST_TEMP_SOURCES})
  endif()

  if(RUST_EXCLUDE_DEPENDS)
    list(REMOVE_ITEM RUST_SOURCES ${RUST_EXCLUDE_DEPENDS})
  endif()

  message(VERBOSE "  RUST_SOURCES: ${RUST_SOURCES}")

  if(RUST_CARGO_TARGET)
    set(RUST_TARGET_ARG --target ${RUST_CARGO_TARGET})
  endif()

  if(NOT RUST_TARGET_NAME)
    set(RUST_TARGET_NAME "${RUST_NAME}-rust")
  endif()

  set(CUSTOM_TARGET_NAME "cargo-${RUST_TARGET_NAME}")
  message(VERBOSE "  Rust target name: ${RUST_TARGET_NAME}")

  set(RUST_DEFAULT_TARGET_PATH "" CACHE STRING "The rust target folder to use, uses the 'target' folder in the shards root if left empty")

  if(NOT RUST_TARGET_PATH)
    if(RUST_DEFAULT_TARGET_PATH)
      file(MAKE_DIRECTORY ${RUST_DEFAULT_TARGET_PATH})
      set(RUST_TARGET_PATH ${RUST_DEFAULT_TARGET_PATH})
    elseif(DEFINED ENV{CARGO_TARGET_DIR})
      set(RUST_TARGET_PATH $ENV{CARGO_TARGET_DIR})
    else()
      set(RUST_TARGET_PATH ${CMAKE_BINARY_DIR}/target)
    endif()
  endif()

  message(VERBOSE "  Rust target path: ${RUST_TARGET_PATH}")

  # Derive lib name
  # - Replace - with _
  # - add libXYZ prefix
  # - add .a or .lib
  set(GENERATED_LIB_NAME ${RUST_LIB_PREFIX}${RUST_NAME}${RUST_LIB_SUFFIX})
  string(REPLACE "-" "_" GENERATED_LIB_NAME "${GENERATED_LIB_NAME}")
  set(GENERATED_LIB_PATH ${RUST_TARGET_PATH}/${RUST_BUILD_SUBDIR}/${GENERATED_LIB_NAME})
  message(VERBOSE "  Rust generated lib path: ${GENERATED_LIB_PATH}")

  set(RUST_CRATE_TYPE_ARG --crate-type staticlib)

  # When the compiler can't automatically provide include paths (emscripten):
  # pass the sysroot to the bindgen clang arguments
  if(EMSCRIPTEN_SYSROOT)
    file(TO_CMAKE_PATH "${EMSCRIPTEN_SYSROOT}" TMP_SYSROOT)
    list(APPEND EXTRA_CLANG_ARGS "--sysroot=${TMP_SYSROOT}")
    list(APPEND EXTRA_CLANG_ARGS "-isystem${TMP_SYSROOT}/include/compat")
  elseif(EMSCRIPTEN)
    # Auto-detect emscripten sysroot - CMAKE_INSTALL_PREFIX points to cache/sysroot
    if(EXISTS "${CMAKE_INSTALL_PREFIX}/include")
      file(TO_CMAKE_PATH "${CMAKE_INSTALL_PREFIX}" TMP_SYSROOT)
      list(APPEND EXTRA_CLANG_ARGS "--sysroot=${TMP_SYSROOT}")
      list(APPEND EXTRA_CLANG_ARGS "-isystem${TMP_SYSROOT}/include/compat")
    endif()
  elseif(CMAKE_SYSROOT)
    file(TO_CMAKE_PATH "${CMAKE_SYSROOT}" TMP_SYSROOT)
    list(APPEND EXTRA_CLANG_ARGS "--sysroot=${CMAKE_SYSROOT}")
  endif()

  if(EMSCRIPTEN)
    # Required to have some symbols be exported
    # https://github.com/rust-lang/rust-bindgen/issues/751
    list(APPEND EXTRA_CLANG_ARGS "-fvisibility=default")
  endif()

  if(IOS)
    list(APPEND EXTRA_CLANG_ARGS "-mios-version-min=10.0")

    if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64" AND XCODE_SDK MATCHES ".*simulator$")
      list(APPEND EXTRA_CLANG_ARGS "--target=aarch64-apple-ios-simulator")
    else()
      list(APPEND EXTRA_CLANG_ARGS "--target=aarch64-apple-ios")
    endif()
  endif()

  if(VISIONOS)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64" AND XCODE_SDK MATCHES ".*simulator$")
      list(APPEND EXTRA_CLANG_ARGS "--target=aarch64-apple-xros-simulator")
    else()
      list(APPEND EXTRA_CLANG_ARGS "--target=aarch64-apple-xros")
    endif()
  endif()

  if(WATCHOS)  
    if(CMAKE_OSX_SYSROOT MATCHES ".*Simulator.*")  
      # Simulator builds  
      if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64" OR CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")  
        list(APPEND EXTRA_CLANG_ARGS "--target=aarch64-apple-watchos-simulator")  
        set(RUST_TARGET "aarch64-apple-watchos-sim")  
      else()  
        list(APPEND EXTRA_CLANG_ARGS "--target=x86_64-apple-watchos-simulator")  
        set(RUST_TARGET "x86_64-apple-watchos-sim")  
      endif()  
    else()  
      # Device builds  
      if(CMAKE_OSX_ARCHITECTURES MATCHES "arm64_32")  
        list(APPEND EXTRA_CLANG_ARGS "--target=arm64_32-apple-watchos")  
        set(RUST_TARGET "arm64_32-apple-watchos")  
      elseif(CMAKE_OSX_ARCHITECTURES MATCHES "armv7k")  
        list(APPEND EXTRA_CLANG_ARGS "--target=armv7k-apple-watchos")  
        set(RUST_TARGET "armv7k-apple-watchos")  
      else()  
        # Newer watches with full 64-bit  
        list(APPEND EXTRA_CLANG_ARGS "--target=aarch64-apple-watchos")  
        set(RUST_TARGET "aarch64-apple-watchos")  
      endif()  
    endif()  
  endif()  

  set(_RUST_ENVIRONMENT ${RUST_ENVIRONMENT})

  # Pass CMAKE_BINARY_DIR so Rust build scripts can find CPM dependencies
  list(APPEND _RUST_ENVIRONMENT "CMAKE_BINARY_DIR=${CMAKE_BINARY_DIR}")

  # Pass SDL3 include path for bindgen to find SDL headers
  # Try SDL3_SOURCE_DIR first, fall back to CPM cache variable
  if(SDL3_SOURCE_DIR)
    list(APPEND _RUST_ENVIRONMENT "SDL3_INCLUDE_PATH=${SDL3_SOURCE_DIR}/include")
  elseif(CPM_PACKAGE_SDL3_SOURCE_DIR)
    list(APPEND _RUST_ENVIRONMENT "SDL3_INCLUDE_PATH=${CPM_PACKAGE_SDL3_SOURCE_DIR}/include")
  endif()

  if(APPLE)
    if(IOS)
      list(APPEND _RUST_ENVIRONMENT "IPHONEOS_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET}")
    elseif(VISIONOS)
      list(APPEND _RUST_ENVIRONMENT "XROS_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET}")
    elseif(WATCHOS)
      list(APPEND _RUST_ENVIRONMENT "WATCHOS_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET}")
    else()
      list(APPEND _RUST_ENVIRONMENT "MACOSX_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET}")
    endif()
  endif()

  if(RUST_TARGET_PATH)
    list(APPEND _RUST_ENVIRONMENT "CARGO_TARGET_DIR=${RUST_TARGET_PATH}")
  endif()

  if(ANDROID_TOOLCHAIN_ROOT)
    list(APPEND _RUST_ENVIRONMENT
      "AR=${ANDROID_TOOLCHAIN_ROOT}/bin/llvm-ar"
      "TARGET_CC=${CMAKE_C_COMPILER}")
  endif()

  if(ANDROID)
    list(APPEND RUST_FLAGS -Ctarget-feature=+fp16)
  endif()

  if(EMSCRIPTEN_ROOT_PATH)
    list(APPEND _RUST_ENVIRONMENT
      "--modify" "PATH=path_list_append:${EMSCRIPTEN_ROOT_PATH}"
      "TARGET_CC=${CMAKE_C_COMPILER}"
      "TARGET_AR=${CMAKE_AR}"
    )
  endif()

  # Zig cross-compilation: configure Rust to use Zig as linker
  if(ZIG_MUSL AND ZIG_WRAPPER_DIR AND ZIG_EXE AND ZIG_TARGET)
    # Configure Rust linker via CARGO_TARGET_<TARGET>_LINKER environment variable
    string(TOUPPER "${RUST_CARGO_TARGET}" _ZIG_RUST_TARGET_UPPER)
    string(REPLACE "-" "_" _ZIG_RUST_TARGET_UPPER "${_ZIG_RUST_TARGET_UPPER}")
    list(APPEND _RUST_ENVIRONMENT
      "CARGO_TARGET_${_ZIG_RUST_TARGET_UPPER}_LINKER=${ZIG_WRAPPER_DIR}/zig-cc.sh"
      "ZIG_EXE=${ZIG_EXE}"
      "ZIG_TARGET=${ZIG_TARGET}"
      "CC=${ZIG_WRAPPER_DIR}/zig-cc.sh"
      "CXX=${ZIG_WRAPPER_DIR}/zig-cxx.sh"
      "AR=${ZIG_WRAPPER_DIR}/zig-ar.sh"
    )

    # Use ZIG_LIB_DIR from Zig.cmake (cached, extracted from 'zig env')
    if(NOT ZIG_LIB_DIR)
      message(FATAL_ERROR "ZIG_LIB_DIR not set - Zig.cmake must be loaded first")
    endif()
    set(_ZIG_LIB_DIR "${ZIG_LIB_DIR}")

    # Map ZIG_ARCH to the correct include directory name
    # Note: riscv64 uses "riscv-linux-any", not "riscv64-linux-any"
    if(ZIG_ARCH STREQUAL "riscv64")
      set(_ZIG_ARCH_ANY "riscv-linux-any")
    elseif(ZIG_ARCH STREQUAL "riscv32")
      set(_ZIG_ARCH_ANY "riscv-linux-any")
    else()
      set(_ZIG_ARCH_ANY "${ZIG_ARCH}-linux-any")
    endif()

    # Pass include paths to bindgen for Zig cross-compilation
    # bindgen uses libclang, which defaults to host system headers
    # We provide -nostdinc and explicit include paths for the target
    # Build the string directly to avoid CMake list/escaping issues
    set(_ZIG_BINDGEN_ARGS "-nostdinc -isystem ${_ZIG_LIB_DIR}/include -isystem ${_ZIG_LIB_DIR}/libc/include/${ZIG_TARGET} -isystem ${_ZIG_LIB_DIR}/libc/include/generic-musl -isystem ${_ZIG_LIB_DIR}/libc/include/${_ZIG_ARCH_ANY} -isystem ${_ZIG_LIB_DIR}/libc/include/any-linux-any")
    # For riscv, clang doesn't recognize the 'gc' suffix in target triples - need explicit --target without 'gc'
    if(ZIG_ARCH STREQUAL "riscv32")
      set(_ZIG_BINDGEN_ARGS "${_ZIG_BINDGEN_ARGS} --target=riscv32-unknown-linux-musl")
    elseif(ZIG_ARCH STREQUAL "riscv64")
      set(_ZIG_BINDGEN_ARGS "${_ZIG_BINDGEN_ARGS} --target=riscv64-unknown-linux-musl")
    endif()
    # Add to environment directly, bypassing the EXTRA_CLANG_ARGS list mechanism
    list(APPEND _RUST_ENVIRONMENT "BINDGEN_EXTRA_CLANG_ARGS=${_ZIG_BINDGEN_ARGS}")
    message(STATUS "Zig bindgen include paths configured for ${ZIG_TARGET}")
  endif()

  # Set BINDGEN_EXTRA_CLANG_ARGS after all platform-specific args are added (non-Zig)
  if(NOT ZIG_MUSL AND EXTRA_CLANG_ARGS)
    set(BINDGEN_EXTRA_CLANG_ARGS BINDGEN_EXTRA_CLANG_ARGS="${EXTRA_CLANG_ARGS}")
  elseif(NOT ZIG_MUSL)
    set(BINDGEN_EXTRA_CLANG_ARGS)
  else()
    # For Zig, BINDGEN_EXTRA_CLANG_ARGS is in _RUST_ENVIRONMENT
    set(BINDGEN_EXTRA_CLANG_ARGS)
  endif()

  list(APPEND _RUST_ENVIRONMENT RUSTFLAGS="${RUST_FLAGS}")

  add_custom_command(
    OUTPUT ${GENERATED_LIB_PATH} ${RUST_OUTPUTS}
    COMMAND ${CMAKE_COMMAND} -E rm -f ${GENERATED_LIB_PATH}
    COMMAND ${CMAKE_COMMAND} -E env ${BINDGEN_EXTRA_CLANG_ARGS} ${_RUST_ENVIRONMENT} ${RUST_BUILD_SCRIPT} ${CARGO_EXE} ${RUST_TOOLCHAIN_OVERRIDE} rustc ${RUST_CARGO_UNSTABLE_FLAGS} ${RUSTC_FLAGS} ${RUST_FEATURES_ARG} ${RUST_CRATE_TYPE_ARG} ${RUST_TARGET_ARG} ${RUST_CARGO_FLAGS_INT}
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

  file(REAL_PATH ${RUST_PROJECT_PATH} RUST_PROJECT_PATH_ABS)
  set_property(TARGET ${RUST_TARGET_NAME} PROPERTY RUST_PROJECT_PATH ${RUST_PROJECT_PATH_ABS})
  set_property(TARGET ${RUST_TARGET_NAME} PROPERTY RUST_NAME ${RUST_NAME})
  set_property(TARGET ${RUST_TARGET_NAME} PROPERTY RUST_FEATURES ${RUST_FEATURES})
  set_property(TARGET ${RUST_TARGET_NAME} PROPERTY RUST_ENVIRONMENT ${RUST_ENVIRONMENT})

  # Store absolute dependency paths
  foreach(SRC_DEP ${RUST_SOURCES} ${RUST_DEPENDS})
    file(REAL_PATH ${SRC_DEP} SRC_DEP_ABS)
    list(APPEND RUST_SOURCES_ABS ${SRC_DEP_ABS})
  endforeach()

  message(VERBOSE "  deps: ${RUST_SOURCES_ABS}")

  set_property(TARGET ${RUST_TARGET_NAME} PROPERTY RUST_DEPENDS ${RUST_SOURCES_ABS})

  # Add default required libraries for windows
  if(WIN32)
    target_link_libraries(${RUST_TARGET_NAME} INTERFACE NtDll Userenv)
  endif()
endfunction()

function(rust_copy_cargo_lock TARGET FILE)
  file(REAL_PATH ${FILE} FILE_ABS)

  if(EXISTS ${FILE_ABS})
    get_property(RUST_PROJECT_PATH TARGET "${TARGET}-rust" PROPERTY RUST_PROJECT_PATH)

    if(NOT EXISTS ${RUST_PROJECT_PATH})
      message(FATAL_ERROR "Invalid rust project ${TARGET}")
    endif()

    file(COPY_FILE ${FILE_ABS} ${RUST_PROJECT_PATH}/Cargo.lock ONLY_IF_DIFFERENT)
  else()
    message(WARNING "Cargo.lock file not found at ${FILE_ABS}")
  endif()

  add_custom_target(${TARGET}-cargo-update
    DEPENDS ${RUST_PROJECT_PATH}/Cargo.lock
    WORKING_DIRECTORY ${RUST_PROJECT_PATH}
    COMMAND ${CARGO_EXE} update
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${RUST_PROJECT_PATH}/Cargo.lock ${FILE_ABS}
    USES_TERMINAL
  )
  add_custom_target(${TARGET}-copy-cargo-lock
    DEPENDS ${RUST_PROJECT_PATH}/Cargo.lock
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${RUST_PROJECT_PATH}/Cargo.lock ${FILE_ABS}
    USES_TERMINAL
  )
endfunction()
