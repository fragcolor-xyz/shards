unset(NETWORK_RUST_TARGETS)
set(NETWORK_REGISTER_SHARDS network_common)
set(NETWORK_SOURCES network_common.cpp)

if(NOT EMSCRIPTEN)
  list(APPEND NETWORK_SOURCES network_kcp.cpp network_stun.cpp)
  list(APPEND NETWORK_REGISTER_SHARDS network_kcp network_stun)
endif()

if(NOT SHARDS_NO_WEBSOCKETS)
  list(APPEND NETWORK_SOURCES network_ws.cpp)
  list(APPEND NETWORK_REGISTER_SHARDS network_ws)

  # Need rust library for WebSocket implementation details
  if(NOT EMSCRIPTEN)
    # Use rustls for Zig cross-compilation (no OpenSSL dependency)
    # Use native-tls for regular builds (uses system TLS)
    if(ZIG_MUSL)
      set(NETWORK_TLS_FEATURE rustls)
    else()
      set(NETWORK_TLS_FEATURE native-tls)
    endif()

    add_rust_library(NAME shards-network
      PROJECT_PATH ${CMAKE_CURRENT_LIST_DIR}
      FEATURES ${NETWORK_TLS_FEATURE})
    list(APPEND NETWORK_RUST_TARGETS shards-network-rust)
  endif()

  # Only need OpenSSL for native-tls builds (not Zig/rustls)
  if((ANDROID OR DESKTOP_LINUX) AND NOT ZIG_MUSL)
    set(NEED_OPENSSL_SYS ON)
  endif()
endif()

add_shards_module(network SOURCES ${NETWORK_SOURCES}
  RUST_TARGETS ${NETWORK_RUST_TARGETS}
  REGISTER_SHARDS ${NETWORK_REGISTER_SHARDS})

target_link_libraries(shards-module-network Tracy::TracyClient)

if(NOT EMSCRIPTEN)
  if(USE_UBSAN)
    target_compile_options(kcp PRIVATE -fno-sanitize=null)
  endif()

  if(NEED_OPENSSL_SYS)
    target_link_libraries(shards-network-rust INTERFACE OpenSSL)
  endif()
  target_link_libraries(shards-module-network kcp-wrapper)
else()
  target_link_libraries(shards-module-network websocket.js)
endif()
