add_library(shards-fast-string STATIC
    storage.cpp)

target_link_libraries(shards-fast-string Boost::container Tracy::TracyClient)
target_include_directories(shards-fast-string PUBLIC ${SHARDS_DIR})