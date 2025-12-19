# Use different implementation for freestanding builds
if(SH_FREESTANDING)
  add_library(shards-fast-string STATIC
      storage_freestanding.cpp)
else()
  add_library(shards-fast-string STATIC
      storage.cpp)
endif()

target_compile_features(shards-fast-string PUBLIC cxx_std_20)

# Dependencies differ based on build type
if(SH_FREESTANDING)
  # Freestanding: minimal dependencies, no Boost::container or TBB
  target_link_libraries(shards-fast-string Tracy::TracyClient)
else()
  target_link_libraries(shards-fast-string Boost::container Tracy::TracyClient TBB::tbb)
endif()
target_include_directories(shards-fast-string PUBLIC ${SHARDS_DIR})

# Set compile flags for storage implementations
if(SH_FREESTANDING)
  set_source_files_properties(
    storage_freestanding.cpp
    PROPERTIES
      COMPILE_FLAGS -O3
      SKIP_PRECOMPILE_HEADERS ON)
else()
  set_source_files_properties(
    storage.cpp
    PROPERTIES
      COMPILE_FLAGS -O3
      SKIP_PRECOMPILE_HEADERS ON)
endif()