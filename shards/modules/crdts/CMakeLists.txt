set(SOURCES
  crdts.cpp
)

add_shards_module(crdts SOURCES ${SOURCES} REGISTER_SHARDS crdts)

target_link_libraries(shards-module-crdts crdt_lite Boost::uuid)
