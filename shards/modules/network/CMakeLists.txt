if(NOT EMSCRIPTEN)
  set(SOURCES network.cpp)
else()
  set(SOURCES network_emscripten.cpp)
endif()

unset(NETWORK_RUST_TARGETS)
unset(REGISTER_SHARDS)
if(NOT EMSCRIPTEN)
  add_rust_library(NAME shards-network
    PROJECT_PATH ${CMAKE_CURRENT_LIST_DIR})
  list(APPEND NETWORK_RUST_TARGETS shards-network-rust)
  list(APPEND REGISTER_SHARDS network rust)
else()
  list(APPEND REGISTER_SHARDS network_em)
endif()

add_shards_module(network SOURCES ${SOURCES}
  RUST_TARGETS ${NETWORK_RUST_TARGETS}
  REGISTER_SHARDS ${REGISTER_SHARDS})

if(NOT EMSCRIPTEN)
  target_include_directories(shards-module-network PRIVATE ../core)

  if(USE_UBSAN)
    target_compile_options(kcp PRIVATE -fno-sanitize=null)
  endif()

  target_link_libraries(shards-network-rust INTERFACE OpenSSL)
  target_link_libraries(shards-module-network OpenSSL kcp-wrapper)
endif()
