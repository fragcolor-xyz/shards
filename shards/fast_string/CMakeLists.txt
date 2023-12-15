add_library(shards-fast-string STATIC
    storage.cpp)

target_link_libraries(shards-fast-string Boost::container Tracy::TracyClient)