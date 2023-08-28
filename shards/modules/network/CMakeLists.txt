if(NOT EMSCRIPTEN)
  set(SOURCES
    network.cpp
  )

  add_rust_library(NAME shards-network
    PROJECT_PATH ${CMAKE_CURRENT_LIST_DIR})

  add_shards_module(network SOURCES ${SOURCES}
    RUST_TARGETS shards-network-rust
    REGISTER_SHARDS network rust)

  target_include_directories(shards-module-network PRIVATE ../core)

  if(USE_UBSAN)
    target_compile_options(shards-module-network PRIVATE -fno-sanitize=null)
  endif()

  target_link_libraries(shards-network-rust INTERFACE OpenSSL)
  target_link_libraries(shards-module-network OpenSSL kcp-wrapper)
endif()
