set(SOURCES
  wasm.cpp
)

add_shards_module(wasm SOURCES ${SOURCES}
  REGISTER_SHARDS wasm
  EXPERIMENTAL)

target_include_directories(shards-module-wasm PRIVATE ../core)
target_link_libraries(shards-module-wasm m3)
