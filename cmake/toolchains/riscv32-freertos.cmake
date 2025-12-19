# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2020-2024 Fragcolor Pte. Ltd.
#
# CMake toolchain file for RISC-V 32-bit bare-metal/FreeRTOS builds
# using xpack-riscv-none-elf-gcc toolchain
#
# Usage:
#   cmake -B build-riscv32 \
#     -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/riscv32-freertos.cmake \
#     -DFREERTOS_CONFIG_DIR=/path/to/your/FreeRTOSConfig.h
#

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR riscv32)

# Toolchain path - user can override via RISCV_TOOLCHAIN_PATH
if(NOT RISCV_TOOLCHAIN_PATH)
  # Default to xpack location
  set(RISCV_TOOLCHAIN_PATH "$ENV{HOME}/devel/xpack-riscv-none-elf-gcc-15.2.0-1")
endif()

# Verify toolchain exists
if(NOT EXISTS "${RISCV_TOOLCHAIN_PATH}/bin/riscv-none-elf-gcc")
  message(FATAL_ERROR "RISC-V toolchain not found at ${RISCV_TOOLCHAIN_PATH}\n"
    "Please set RISCV_TOOLCHAIN_PATH to your xpack-riscv-none-elf-gcc installation")
endif()

# Compilers
set(CMAKE_C_COMPILER "${RISCV_TOOLCHAIN_PATH}/bin/riscv-none-elf-gcc")
set(CMAKE_CXX_COMPILER "${RISCV_TOOLCHAIN_PATH}/bin/riscv-none-elf-g++")
set(CMAKE_ASM_COMPILER "${RISCV_TOOLCHAIN_PATH}/bin/riscv-none-elf-gcc")
set(CMAKE_AR "${RISCV_TOOLCHAIN_PATH}/bin/riscv-none-elf-ar" CACHE FILEPATH "Archiver")
set(CMAKE_RANLIB "${RISCV_TOOLCHAIN_PATH}/bin/riscv-none-elf-ranlib" CACHE FILEPATH "Ranlib")
set(CMAKE_OBJCOPY "${RISCV_TOOLCHAIN_PATH}/bin/riscv-none-elf-objcopy" CACHE FILEPATH "Objcopy")
set(CMAKE_OBJDUMP "${RISCV_TOOLCHAIN_PATH}/bin/riscv-none-elf-objdump" CACHE FILEPATH "Objdump")
set(CMAKE_SIZE "${RISCV_TOOLCHAIN_PATH}/bin/riscv-none-elf-size" CACHE FILEPATH "Size")

# rv32gc = rv32imafdc (G = IMAFD + Zicsr + Zifencei, C = compressed)
set(RISCV_ARCH "rv32imafdc" CACHE STRING "RISC-V architecture")
set(RISCV_ABI "ilp32d" CACHE STRING "RISC-V ABI")

# Compiler flags
# Note: exceptions are required by shards code, RTTI is disabled for size
set(CMAKE_C_FLAGS_INIT "-march=${RISCV_ARCH} -mabi=${RISCV_ABI} -ffunction-sections -fdata-sections")
set(CMAKE_CXX_FLAGS_INIT "-march=${RISCV_ARCH} -mabi=${RISCV_ABI} -ffunction-sections -fdata-sections -fno-rtti")
set(CMAKE_ASM_FLAGS_INIT "-march=${RISCV_ARCH} -mabi=${RISCV_ABI}")

# Bare-metal linker flags
set(CMAKE_EXE_LINKER_FLAGS_INIT "-nostartfiles -specs=nosys.specs -Wl,--gc-sections")

# Enable freestanding mode - this triggers module skipping in Platform.cmake
set(SH_FREESTANDING ON CACHE BOOL "Freestanding/bare-metal build" FORCE)

# Skip CMake compiler checks (no OS to run test executables on)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# Search paths
set(CMAKE_FIND_ROOT_PATH "${RISCV_TOOLCHAIN_PATH}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

message(STATUS "RISC-V toolchain: ${RISCV_TOOLCHAIN_PATH}")
message(STATUS "RISC-V arch: ${RISCV_ARCH}, ABI: ${RISCV_ABI}")
