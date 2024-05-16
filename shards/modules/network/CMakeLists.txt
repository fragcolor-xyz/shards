unset(NETWORK_RUST_TARGETS)
set(NETWORK_REGISTER_SHARDS network_common)
set(NETWORK_SOURCES network_common.cpp)

if(NOT EMSCRIPTEN)
  # list(APPEND NETWORK_SOURCES network_kcp.cpp)
  # list(APPEND NETWORK_REGISTER_SHARDS network_kcp)
endif()

list(APPEND NETWORK_SOURCES network_ws.cpp)
list(APPEND NETWORK_REGISTER_SHARDS network_ws)

# Need rust library for WebSocket implementation details
if(NOT EMSCRIPTEN)
  add_rust_library(NAME shards-network
    PROJECT_PATH ${CMAKE_CURRENT_LIST_DIR})
  list(APPEND NETWORK_RUST_TARGETS shards-network-rust)
endif()

add_shards_module(network SOURCES ${NETWORK_SOURCES}
  RUST_TARGETS ${NETWORK_RUST_TARGETS}
  REGISTER_SHARDS ${NETWORK_REGISTER_SHARDS})

if(NOT EMSCRIPTEN)
  if(USE_UBSAN)
    target_compile_options(kcp PRIVATE -fno-sanitize=null)
  endif()

  target_link_libraries(shards-network-rust INTERFACE OpenSSL)
  target_link_libraries(shards-module-network OpenSSL kcp-wrapper)
endif()
