if(UNIX AND NOT APPLE)
  set(LINUX TRUE)
endif()

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
  add_compile_definitions(CPUBITS64)
elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
  add_compile_definitions(CPUBITS32)
endif()

# Default arch if ARCH is not set
if(NOT ARCH)
  if(X86 AND NOT EMSCRIPTEN)
    set(ARCH "sandybridge")
  endif()
endif()

if(ARCH)
  add_compile_options(-march=${ARCH})
endif()

if(USE_FPIC)
  set(CMAKE_POSITION_INDEPENDENT_CODE ON)
  list(APPEND EXTERNAL_CMAKE_ARGS -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DSDL_STATIC_PIC=ON)
endif()

if(EMSCRIPTEN)
  if(CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    add_compile_options(-g1 -Os)
  endif()

  if(CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo" OR CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_link_options("SHELL:-s ASSERTIONS=2")
  endif()

  add_compile_definitions(NO_FORCE_INLINE)
  add_link_options(--bind)

  ## if we wanted thread support...
  if(EMSCRIPTEN_PTHREADS)
    add_link_options("SHELL:-s USE_PTHREADS=1 -s PTHREAD_POOL_SIZE=6")
    add_compile_options(-pthread -Wno-pthreads-mem-growth)
    add_link_options(-pthread)
  endif()

  if(EMSCRIPTEN_IDBFS)
    add_link_options(-lidbfs.js)
  endif()

  if(EMSCRIPTEN_NODEFS)
    add_link_options(-lnodefs.js)
  endif()
endif()

# mingw32 static runtime
if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
  if(WIN32)
    add_link_options(-static)
    list(APPEND EXTERNAL_CMAKE_ARGS -DCMAKE_CXX_FLAGS=-static -DCMAKE_C_FLAGS=-static)
  endif()
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
  # aggressive inlining
  if(NOT SKIP_HEAVY_INLINE)
    # this works with emscripten too but makes the final binary much bigger
    # for now let's keep it disabled
    if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
      set(INLINING_FLAGS -mllvm -inline-threshold=100000)
    elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
      set(INLINING_FLAGS -finline-limit=100000)
    elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Intel")
      # using Intel C++
    elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
      # using Visual Studio C++
    endif()
  endif()
endif()

if(USE_LTO)
  add_compile_options(-flto)
  add_link_options(-flto)
endif()

add_compile_options(-ffast-math -fno-finite-math-only -funroll-loops -Wno-multichar)

if((WIN32 AND CMAKE_BUILD_TYPE STREQUAL "Debug") OR FORCE_USE_LLD)
  add_link_options(-fuse-ld=lld)
  SET(CMAKE_AR llvm-ar)
  SET(CMAKE_RANLIB llvm-ranlib)
endif()

if(LINUX)
  add_link_options(-export-dynamic)
  if(USE_VALGRIND)
    add_compile_definitions(BOOST_USE_VALGRIND)
  endif()
endif()

if(USE_GCC_ANALYZER)
  if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    add_compile_options(-fanalyzer)
  endif()
endif()

if(PROFILE_GPROF)
  add_compile_options(-pg -DNO_FORCE_INLINE)
  add_link_options(-pg)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  if(NOT WIN32 AND NOT EMSCRIPTEN)
    add_compile_options(-fsanitize=undefined)
    add_link_options(-fsanitize=undefined)
    add_compile_definitions(CB_USE_UBSAN)
  endif()
endif()

if(CODE_COVERAGE)
  if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    add_compile_options(--coverage)
    add_link_options(--coverage)
  endif()
endif()

# Move this?
if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
  if(NOT CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    add_compile_definitions(SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_INFO)
  else()
    add_compile_definitions(SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_DEBUG)
  endif()
else()
  add_compile_definitions(SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE)
endif()

# Move this?
if(VERBOSE_BGFX_LOGGING)
  add_compile_definitions(BGFX_CONFIG_DEBUG)
endif()

add_compile_definitions(BOOST_INTERPROCESS_BOOTSTAMP_IS_LASTBOOTUPTIME=1)
add_compile_options(-Wall)

if(APPLE)
  # This tells FindPackage(Threads) that threads are built in
  set(CMAKE_THREAD_LIBS_INIT "-lpthread")
  set(CMAKE_HAVE_THREADS_LIBRARY 1)
  set(CMAKE_USE_WIN32_THREADS_INIT 0)
  set(CMAKE_USE_PTHREADS_INIT 1)
  set(THREADS_PREFER_PTHREAD_FLAG ON)

  add_compile_definitions(BOOST_STACKTRACE_GNU_SOURCE_NOT_REQUIRED)
  add_compile_options(-Wextra -Wno-unused-parameter -Wno-missing-field-initializers)
endif()
