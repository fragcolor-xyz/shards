set(SOURCES
  brotli.cpp
)

add_shards_module(brotli SOURCES ${SOURCES}
  REGISTER_SHARDS brotli)

target_link_libraries(shards-module-brotli brotlienc-static brotlidec-static brotlicommon-static)
