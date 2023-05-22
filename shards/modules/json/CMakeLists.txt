set(SOURCES
  json.cpp
)

add_shards_module(json SOURCES ${SOURCES}
  REGISTER_SHARDS json)

target_link_libraries(shards-module-json nlohmann_json)
