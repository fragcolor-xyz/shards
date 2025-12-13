# Zig Cross-Compilation Toolchain
# Usage: cmake -DCMAKE_TOOLCHAIN_FILE=cmake/Zig.cmake -DZIG_TARGET=aarch64-linux-musl ..
#
# Required variables:
#   ZIG_TARGET - Target triple (e.g., aarch64-linux-musl, x86_64-linux-musl)
#
# Optional variables:
#   ZIG_PATH - Path to zig executable directory (or set ZIG_PATH env var)

cmake_minimum_required(VERSION 3.20)

# CRITICAL: Set this FIRST to avoid try_compile needing a working linker
# This makes try_compile create static libraries instead of executables
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# Cache ZIG_TARGET immediately so it persists through try_compile
if(ZIG_TARGET)
  set(ZIG_TARGET "${ZIG_TARGET}" CACHE STRING "Zig target triple" FORCE)
endif()

# Detect if we're in a try_compile context
# CMake creates various temp directories for test compiles - detect them all
set(_ZIG_IN_TRY_COMPILE FALSE)
foreach(_pattern "TryCompile" "CMakeScratch" "_CMakeLTOTest" "CMakeTmp" "CMakeFiles")
  string(FIND "${CMAKE_BINARY_DIR}" "${_pattern}" _found)
  if(_found GREATER -1)
    # Additional check: CMakeFiles is only a try_compile if it's a subdirectory path
    if(_pattern STREQUAL "CMakeFiles")
      string(FIND "${CMAKE_BINARY_DIR}" "CMakeFiles/" _found_slash)
      if(_found_slash GREATER -1)
        set(_ZIG_IN_TRY_COMPILE TRUE)
        break()
      endif()
    else()
      set(_ZIG_IN_TRY_COMPILE TRUE)
      break()
    endif()
  endif()
endforeach()

# Required: ZIG_TARGET must be set (skip check in try_compile context)
if(NOT ZIG_TARGET)
  if(_ZIG_IN_TRY_COMPILE)
    # In try_compile, just return silently - CMake will use cached values
    return()
  endif()
  message(FATAL_ERROR "ZIG_TARGET must be set (e.g., aarch64-linux-musl, x86_64-linux-musl)")
endif()

set(CMAKE_CROSSCOMPILING ON)

# Parse target triple: arch-os-libc (e.g., aarch64-linux-musl)
string(REPLACE "-" ";" ZIG_TARGET_PARTS "${ZIG_TARGET}")
list(GET ZIG_TARGET_PARTS 0 ZIG_ARCH)
list(GET ZIG_TARGET_PARTS 1 ZIG_OS)
list(LENGTH ZIG_TARGET_PARTS ZIG_TARGET_LEN)
if(ZIG_TARGET_LEN GREATER 2)
  list(GET ZIG_TARGET_PARTS 2 ZIG_LIBC)
else()
  set(ZIG_LIBC "")
endif()

message(STATUS "Zig target: ${ZIG_TARGET}")
message(STATUS "  Architecture: ${ZIG_ARCH}")
message(STATUS "  OS: ${ZIG_OS}")
message(STATUS "  Libc: ${ZIG_LIBC}")

# Map to CMake system names
if(ZIG_OS STREQUAL "linux")
  set(CMAKE_SYSTEM_NAME Linux)
elseif(ZIG_OS STREQUAL "windows")
  set(CMAKE_SYSTEM_NAME Windows)
elseif(ZIG_OS STREQUAL "macos")
  set(CMAKE_SYSTEM_NAME Darwin)
elseif(ZIG_OS STREQUAL "freebsd")
  set(CMAKE_SYSTEM_NAME FreeBSD)
else()
  message(FATAL_ERROR "Unknown Zig OS: ${ZIG_OS}")
endif()

# Map architecture
if(ZIG_ARCH STREQUAL "aarch64")
  set(CMAKE_SYSTEM_PROCESSOR aarch64)
elseif(ZIG_ARCH STREQUAL "x86_64")
  set(CMAKE_SYSTEM_PROCESSOR x86_64)
elseif(ZIG_ARCH STREQUAL "x86")
  set(CMAKE_SYSTEM_PROCESSOR i686)
elseif(ZIG_ARCH STREQUAL "arm")
  set(CMAKE_SYSTEM_PROCESSOR arm)
elseif(ZIG_ARCH STREQUAL "riscv64")
  set(CMAKE_SYSTEM_PROCESSOR riscv64)
elseif(ZIG_ARCH STREQUAL "riscv32")
  set(CMAKE_SYSTEM_PROCESSOR riscv32)
else()
  message(FATAL_ERROR "Unknown Zig architecture: ${ZIG_ARCH}")
endif()

# Find Zig executable (cache immediately)
if(NOT ZIG_EXE)
  if(ZIG_PATH)
    find_program(ZIG_EXE NAMES zig PATHS "${ZIG_PATH}" NO_DEFAULT_PATH REQUIRED)
  else()
    find_program(ZIG_EXE NAMES zig PATHS ENV ZIG_PATH REQUIRED)
  endif()
endif()

message(STATUS "Found zig: ${ZIG_EXE}")

# Get zig version for verification
execute_process(
  COMMAND "${ZIG_EXE}" version
  OUTPUT_VARIABLE ZIG_VERSION
  OUTPUT_STRIP_TRAILING_WHITESPACE
)
message(STATUS "Zig version: ${ZIG_VERSION}")

# Get zig lib directory (needed for bindgen sysroot)
# Parse from 'zig env' output which returns JSON-like format
execute_process(
  COMMAND "${ZIG_EXE}" env
  OUTPUT_VARIABLE _ZIG_ENV_OUTPUT
  OUTPUT_STRIP_TRAILING_WHITESPACE
)
# Extract lib_dir from output: .lib_dir = "/path/to/lib/zig",
string(REGEX MATCH "\\.lib_dir = \"([^\"]+)\"" _ZIG_LIB_MATCH "${_ZIG_ENV_OUTPUT}")
if(_ZIG_LIB_MATCH)
  set(ZIG_LIB_DIR "${CMAKE_MATCH_1}" CACHE PATH "Zig lib directory" FORCE)
  message(STATUS "Zig lib dir: ${ZIG_LIB_DIR}")
else()
  message(WARNING "Could not determine Zig lib directory from 'zig env'")
endif()

# Use a stable wrapper directory based on source dir + target
# This ensures try_compile uses the same wrappers
set(ZIG_WRAPPER_DIR "${CMAKE_CURRENT_LIST_DIR}/zig-wrappers/${ZIG_TARGET}" CACHE PATH "Zig wrapper scripts directory" FORCE)

# Only generate wrappers once (check if they exist and are correct)
set(_ZIG_CC_WRAPPER "${ZIG_WRAPPER_DIR}/zig-cc.sh")
set(_ZIG_WRAPPER_NEEDS_UPDATE FALSE)

if(NOT EXISTS "${_ZIG_CC_WRAPPER}")
  set(_ZIG_WRAPPER_NEEDS_UPDATE TRUE)
else()
  # Check if the zig path in the wrapper matches current ZIG_EXE
  file(READ "${_ZIG_CC_WRAPPER}" _ZIG_CC_CONTENT)
  if(NOT _ZIG_CC_CONTENT MATCHES "${ZIG_EXE}")
    set(_ZIG_WRAPPER_NEEDS_UPDATE TRUE)
  endif()
endif()

if(_ZIG_WRAPPER_NEEDS_UPDATE)
  message(STATUS "Generating Zig wrapper scripts in ${ZIG_WRAPPER_DIR}")
  file(MAKE_DIRECTORY "${ZIG_WRAPPER_DIR}")

  # Generate zig-cc wrapper
  file(WRITE "${ZIG_WRAPPER_DIR}/zig-cc.sh" "#!/bin/bash
# Auto-generated Zig C compiler wrapper for ${ZIG_TARGET}
exec \"${ZIG_EXE}\" cc --target=\"${ZIG_TARGET}\" \"$@\"
")

  # Generate zig-c++ wrapper
  file(WRITE "${ZIG_WRAPPER_DIR}/zig-cxx.sh" "#!/bin/bash
# Auto-generated Zig C++ compiler wrapper for ${ZIG_TARGET}
exec \"${ZIG_EXE}\" c++ --target=\"${ZIG_TARGET}\" \"$@\"
")

  # Generate zig-ar wrapper
  file(WRITE "${ZIG_WRAPPER_DIR}/zig-ar.sh" "#!/bin/bash
# Auto-generated Zig archive tool wrapper
exec \"${ZIG_EXE}\" ar \"$@\"
")

  # Generate zig-ranlib wrapper (no-op, zig ar handles it)
  file(WRITE "${ZIG_WRAPPER_DIR}/zig-ranlib.sh" "#!/bin/bash
# Zig ar already handles ranlib functionality
exit 0
")

  # Make wrappers executable
  file(CHMOD
    "${ZIG_WRAPPER_DIR}/zig-cc.sh"
    "${ZIG_WRAPPER_DIR}/zig-cxx.sh"
    "${ZIG_WRAPPER_DIR}/zig-ar.sh"
    "${ZIG_WRAPPER_DIR}/zig-ranlib.sh"
    PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
  )
endif()

# Set compilers to generated wrapper scripts
set(CMAKE_C_COMPILER "${ZIG_WRAPPER_DIR}/zig-cc.sh")
set(CMAKE_CXX_COMPILER "${ZIG_WRAPPER_DIR}/zig-cxx.sh")
set(CMAKE_AR "${ZIG_WRAPPER_DIR}/zig-ar.sh")
set(CMAKE_RANLIB "${ZIG_WRAPPER_DIR}/zig-ranlib.sh")

# Zig handles the linker internally
set(CMAKE_C_COMPILER_TARGET "${ZIG_TARGET}")
set(CMAKE_CXX_COMPILER_TARGET "${ZIG_TARGET}")

# Musl static linking configuration
if(ZIG_LIBC STREQUAL "musl")
  set(ZIG_MUSL ON CACHE BOOL "Building with musl libc" FORCE)
  set(GNU_STATIC_BUILD ON CACHE BOOL "Static build" FORCE)
  message(STATUS "Musl target detected - enabling static build")
endif()

# Export variables for Rust.cmake to use (already cached above)
set(ZIG_EXE "${ZIG_EXE}" CACHE FILEPATH "Path to zig executable" FORCE)

# Skip Swift for non-Apple targets
if(NOT CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  set(CMAKE_Swift_COMPILER_WORKS FALSE CACHE BOOL "" FORCE)
endif()

# For Linux targets, set up proper find root
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
  set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
  set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
  set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
endif()
