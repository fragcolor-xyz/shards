add_library(shards-fast-string STATIC
    storage.cpp)

target_compile_features(shards-fast-string PUBLIC cxx_std_20)

target_link_libraries(shards-fast-string Boost::container Tracy::TracyClient)
target_include_directories(shards-fast-string PUBLIC ${SHARDS_DIR})

set_source_files_properties(
  storage.cpp
  PROPERTIES
    COMPILE_FLAGS -O3
    SKIP_PRECOMPILE_HEADERS ON)