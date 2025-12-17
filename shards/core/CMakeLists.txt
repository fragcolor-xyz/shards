set(core_SOURCES
  async.cpp
  ops_internal.cpp
  number_types.cpp
  utils.cpp
  coro.cpp
  wire_dsl.cpp
  trait.cpp
  type_info.cpp
  hash.cpp
  taskflow.cpp
  pmr/shared_temp_allocator.cpp
  log_api.cpp
  wires.cpp
)

if(EMSCRIPTEN)
  list(APPEND core_SOURCES em_proxy.cpp)
endif()

add_library(shards-core STATIC ${core_SOURCES})

target_include_directories(shards-core PUBLIC ${SHARDS_DIR}/include)
target_include_directories(shards-core PUBLIC ${SHARDS_DIR})

# Expose tracy client so all dependencies get the TRACY_ENABLE define
target_link_libraries(shards-core Tracy::TracyClient shards-fast-string)

if(CMAKE_BUILD_TYPE MATCHES Debug)
  set(SHARDS_INLINE_EVERYTHING_DEFAULT OFF)
else()
  set(SHARDS_INLINE_EVERYTHING_DEFAULT ON)
endif()

if(NOT DEFINED SHARDS_INLINE_EVERYTHING)
  set(SHARDS_INLINE_EVERYTHING ${SHARDS_INLINE_EVERYTHING_DEFAULT} CACHE BOOL "When enabled, runtime and core shards are inlined in a single compilation unit")
endif()

if(SHARDS_INLINE_EVERYTHING)
  target_compile_definitions(shards-core PUBLIC SHARDS_INLINE_EVERYTHING=1)
else()
  target_compile_definitions(shards-core PUBLIC SHARDS_INLINE_EVERYTHING=0)
endif()

# we are building with boost containers since we build shards, we can support this
target_compile_definitions(shards-core PUBLIC HAS_BOOST_CONTAINER=1)

if(CMAKE_BUILD_TYPE MATCHES Debug)
  set(SHARDS_DEBUG_THREAD_NAMES_DEFAULT ON)
else()
  set(SHARDS_DEBUG_THREAD_NAMES_DEFAULT OFF)
endif()

set(SHARDS_DEBUG_THREAD_NAMES ${SHARDS_DEBUG_THREAD_NAMES_DEFAULT} CACHE BOOL "When enabled, threads are given names based on running wires")

if(SHARDS_DEBUG_THREAD_NAMES)
  target_compile_definitions(shards-core PUBLIC SH_DEBUG_THREAD_NAMES=1)
endif()

set(SHARDS_THREAD_FIBER OFF CACHE BOOL "When enabled, coroutines are evaluated using a single thread (for debugging purposes)")

if(SHARDS_THREAD_FIBER)
  target_compile_definitions(shards-core PUBLIC SH_USE_THREAD_FIBER=1)
endif()

set(SHARDS_NO_CONSISTENT_RESUMER OFF CACHE BOOL "When enabled, consistent resumer checks are disabled")

# # Uncomment to enable consistent resumer debugging, this might be useful to debug some specific issues when coroutines cannot be resumed on different threads
if(NOT SHARDS_NO_CONSISTENT_RESUMER AND CMAKE_BUILD_TYPE MATCHES Debug AND NOT SHARDS_THREAD_FIBER AND NOT EMSCRIPTEN)
  # Set to non-zero to enable, Set to 2 to enable more detailed logging
  target_compile_definitions(shards-core PUBLIC SH_DEBUG_CONSISTENT_RESUMER=1)
endif()

set(SHARDS_DEBUGGER OFF CACHE BOOL "When enabled, debugger is enabled")

if(SHARDS_DEBUGGER)
  target_compile_definitions(shards-core PUBLIC SHARDS_DEBUGGER=1)
else()
  target_compile_definitions(shards-core PUBLIC SHARDS_DEBUGGER=0)
endif()

if(WIN32)
  if(NOT CMAKE_BUILD_TYPE STREQUAL "Release")
    target_link_libraries(shards-core Boost::stacktrace_windbg)
  else()
    target_link_libraries(shards-core Boost::stacktrace_basic)
  endif()

  # Target windows SDK (https://docs.microsoft.com/en-us/cpp/porting/modifying-winver-and-win32-winnt)
  # 0x0A00 = Windows 10
  target_compile_definitions(shards-core PUBLIC WINVER=0x0A00 _WIN32_WINNT=0x0A00)
elseif(DESKTOP_LINUX)
  target_link_libraries(shards-core Boost::stacktrace_addr2line)
else()
  target_link_libraries(shards-core Boost::stacktrace_basic)
endif()

target_precompile_headers(shards-core PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/pch.cpp>")

if(APPLE)
  target_link_libraries(shards-core
    "-framework Foundation"
    "-framework CoreAudio"
    "-framework CoreVideo"
    "-framework QuartzCore"
    "-framework Accelerate"
    "-framework Security"
    -liconv
  )

  if(NOT WATCHOS)
    target_link_libraries(shards-core
      "-framework AudioToolbox"
      "-framework IOKit"
      "-framework Metal"
      "-framework MetalPerformanceShaders"
      "-framework CoreHaptics"
      "-framework GameController"
      "-framework SystemConfiguration"
    )
  endif()

  if(IOS OR VISIONOS OR WATCHOS)
    target_link_libraries(shards-core
      "-framework AVFoundation"
      "-framework CoreGraphics"
      "-framework UIKit"
    )

    if(NOT SKIP_CORE_MOTION)
      target_link_libraries(shards-core "-framework CoreMotion")
    endif()
  else()
    target_link_libraries(shards-core
      "-framework Cocoa"
      "-framework Carbon"
      "-framework ForceFeedback"
    )
  endif()
endif()

target_link_libraries(shards-core Boost::filesystem Boost::lockfree Boost::foreach Boost::multiprecision Boost::atomic Boost::thread Boost::tti)

# Custom fcontext implementation option
# Default OFF (use custom assembly), ON for Windows (uses Boost::context)
if(WIN32)
  set(SH_USE_BOOST_CONTEXT_DEFAULT ON)
else()
  set(SH_USE_BOOST_CONTEXT_DEFAULT OFF)
endif()

set(SH_USE_BOOST_CONTEXT ${SH_USE_BOOST_CONTEXT_DEFAULT} CACHE BOOL
  "Use Boost.Context instead of custom assembly for fiber support (required on Windows)")

if(NOT EMSCRIPTEN)
  target_link_libraries(shards-core Boost::beast Boost::asio)

  if(SH_USE_BOOST_CONTEXT)
    # Use Boost.Context for coroutines
    message(STATUS "Shards: Using Boost.Context for fiber support")
    target_link_libraries(shards-core Boost::context)
  else()
    # Use custom assembly-based fcontext
    message(STATUS "Shards: Using custom fcontext assembly for fiber support")
    target_compile_definitions(shards-core PUBLIC SH_CUSTOM_FCONTEXT=1)

    # Detect architecture and select appropriate assembly file
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64|amd64")
      if(APPLE)
        set(FCONTEXT_ASM_FILE "${CMAKE_CURRENT_SOURCE_DIR}/asm/fcontext_x86_64_sysv_macho.S")
      else()
        set(FCONTEXT_ASM_FILE "${CMAKE_CURRENT_SOURCE_DIR}/asm/fcontext_x86_64_sysv_elf.S")
      endif()
      message(STATUS "Shards: Selected x86_64 fcontext assembly")
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64|ARM64")
      if(APPLE)
        set(FCONTEXT_ASM_FILE "${CMAKE_CURRENT_SOURCE_DIR}/asm/fcontext_arm64_aapcs_macho.S")
      else()
        set(FCONTEXT_ASM_FILE "${CMAKE_CURRENT_SOURCE_DIR}/asm/fcontext_arm64_aapcs_elf.S")
      endif()
      message(STATUS "Shards: Selected ARM64 fcontext assembly")
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "riscv64|riscv32|RISCV")
      set(FCONTEXT_ASM_FILE "${CMAKE_CURRENT_SOURCE_DIR}/asm/fcontext_riscv_elf.S")
      message(STATUS "Shards: Selected RISC-V fcontext assembly")
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "arm|ARM")
      set(FCONTEXT_ASM_FILE "${CMAKE_CURRENT_SOURCE_DIR}/asm/fcontext_arm_aapcs_elf.S")
      message(STATUS "Shards: Selected ARM32 fcontext assembly")
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "i[36]86|x86")
      set(FCONTEXT_ASM_FILE "${CMAKE_CURRENT_SOURCE_DIR}/asm/fcontext_x86_sysv_elf.S")
      message(STATUS "Shards: Selected x86 (32-bit) fcontext assembly")
    else()
      message(FATAL_ERROR "Unsupported architecture for custom fcontext: ${CMAKE_SYSTEM_PROCESSOR}. "
        "Set SH_USE_BOOST_CONTEXT=ON to use Boost.Context instead.")
    endif()

    # Add assembly file to sources
    target_sources(shards-core PRIVATE ${FCONTEXT_ASM_FILE})

    # Enable ASM language if not already enabled
    enable_language(ASM)
  endif()
else()
  target_include_directories(shards-core PUBLIC $<TARGET_PROPERTY:Boost::asio,INTERFACE_INCLUDE_DIRECTORIES>)

  # For usage of boost/beast/core/detail/base64.hpp
  target_include_directories(shards-core PUBLIC $<TARGET_PROPERTY:Boost::beast,INTERFACE_INCLUDE_DIRECTORIES>)
endif()

target_link_libraries(shards-core
  spdlog magic_enum nameof linalg xxHash
  pdqsort entt utf8.h Taskflow stb nlohmann_json
  shards-logging tbb
)
target_compile_definitions(shards-core PUBLIC SHARDS_WITH_ENTT=1)

if(DESKTOP)
  target_compile_definitions(shards-core PUBLIC SHARDS_DESKTOP=1)
endif()

if(WIN32)
  target_link_libraries(shards-core ws2_32 mswsock Secur32 Crypt32 NCrypt)
elseif(DESKTOP_LINUX)
  target_link_libraries(shards-core pthread dl rt)
endif()

target_compile_options(shards-core
  # -fno-finite-math-only` restores NaN/infinity support while keeping the other fast-math optimizations active
  PRIVATE -ffast-math -fno-finite-math-only -funroll-loops
)

if(EMSCRIPTEN)
  target_link_options(shards-core PUBLIC "-sLLD_REPORT_UNDEFINED")

  option(SHARDS_USE_JSPI "Use JSPI instead of Asyncify for fiber support (requires Chrome 137+/Firefox 139+)" OFF)

  if(SHARDS_USE_JSPI)
    # JSPI-based fiber support (no Asyncify instrumentation)
    message(STATUS "Shards: Using JSPI for fiber support (no Asyncify)")
    target_compile_definitions(shards-core PUBLIC SHARDS_USE_JSPI=1)

    # Link the JSPI fiber library
    target_link_options(shards-core PUBLIC
      "--js-library=${CMAKE_CURRENT_SOURCE_DIR}/shards_fiber.js"
    )

    # Configure JSPI imports (suspending functions) and exports (promising functions)
    target_link_options(shards-core PUBLIC
      "-sJSPI=1"
      "-sJSPI_IMPORTS=['shardsFiberSuspend']"
      "-sJSPI_EXPORTS=['_main','_shardsTick','_shardsLoadScript']"
    )

    # Export runtime methods needed for fiber management
    target_link_options(shards-core PUBLIC
      "-sEXPORTED_RUNTIME_METHODS=['stackSave','stackRestore']"
    )
  elseif(NOT SHARDS_THREAD_FIBER)
    # Asyncify-based fiber support (default for Emscripten)
    message(STATUS "Shards: Using Asyncify for fiber support")
    target_link_options(shards-core PUBLIC "-sASYNCIFY=1")
  endif()

  # Require optimizations when building with asyncify too keep size/build time reasonable
  # if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  # target_compile_options(shards-core PUBLIC "-O3")
  # target_link_options(shards-core PUBLIC "-O3")
  # endif()
endif()

if(DESKTOP_LINUX OR APPLE AND USE_UBSAN)
  target_compile_options(shards-core PUBLIC -fsanitize=undefined)
  target_link_options(shards-core PUBLIC -fsanitize=undefined)
endif()
