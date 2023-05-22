if(NOT EMSCRIPTEN)
  set(SOURCES
    network.cpp
  )

  add_shards_module(network SOURCES ${SOURCES}
    REGISTER_SHARDS network rust)

  target_include_directories(shards-module-network PRIVATE ../core)

  target_link_libraries(shards-module-network OpenSSL kcp-wrapper)

  add_rust_library(NAME shards-network 
  PROJECT_PATH ${CMAKE_CURRENT_LIST_DIR})
  target_link_libraries(shards-module-network shards-network-rust)
endif()
