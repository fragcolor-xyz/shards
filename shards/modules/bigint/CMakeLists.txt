set(SOURCES
  bigint.cpp
)

add_shards_module(bigint SOURCES ${SOURCES}
  REGISTER_SHARDS bigint
  EXPERIMENTAL)

target_include_directories(shards-module-bigint PRIVATE ../core)
target_link_libraries(shards-module-bigint Boost::multiprecision)
