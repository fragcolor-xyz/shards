set(HTTP_SUPPORTED ON)

# emscripten implementation depends on fetch support
if(EMSCRIPTEN)
  if(NODEJS)
    message(STATUS "Http module disabled for node.js due to missing Fetch support")
    set(HTTP_SUPPORTED OFF)
  endif()
endif()

if(HTTP_SUPPORTED)
  # For Zig cross-compilation, use Rust-only implementation (no C++ Server shard)
  # C++ http.cpp requires OpenSSL via Boost.Asio SSL
  if(ZIG_MUSL)
    # Rust-only build with rustls (ring crypto provider to avoid aws-lc-rs C build)
    add_rust_library(NAME shards-http
      PROJECT_PATH ${CMAKE_CURRENT_LIST_DIR}
      FEATURES rustls-ring socks)

    add_shards_module(http
      RUST_TARGETS shards-http-rust
      REGISTER_SHARDS rust)
  else()
    # Full build with C++ and Rust
    set(SOURCES http.cpp)
    set(REGISTER_SHARDS_ARG http)

    # The rust shards use reqwest, which can not run on emscripten
    if(NOT EMSCRIPTEN)
      # rustls (ring provider) + rustls-platform-verifier: cert verification stays
      # platform-native (Security.framework/schannel/native certs) while the TLS
      # record layer avoids deprecated Secure Transport, which produced spurious
      # intermittent "bad record MAC" errors on macOS 26.x.
      add_rust_library(NAME shards-http
        PROJECT_PATH ${CMAKE_CURRENT_LIST_DIR}
        FEATURES rustls-ring socks)
      set(RUST_TARGETS_ARG RUST_TARGETS shards-http-rust)
      list(APPEND REGISTER_SHARDS_ARG rust)
    endif()

    add_shards_module(http SOURCES ${SOURCES}
      ${RUST_TARGETS_ARG}
      REGISTER_SHARDS ${REGISTER_SHARDS_ARG})

    target_include_directories(shards-module-http PRIVATE ../core)

    if(NOT EMSCRIPTEN)
      # Plaintext-only Boost.Beast server/client — no OpenSSL/LibreSSL. TLS for
      # Http.Server is expected to be terminated by a reverse proxy; the Rust
      # client (Http.Get/Post) handles https:// via its own TLS stack.
      target_link_libraries(shards-module-http Boost::beast Boost::asio Boost::context)
    else()
      target_include_directories(shards-module-http PUBLIC $<TARGET_PROPERTY:Boost::asio,INTERFACE_INCLUDE_DIRECTORIES>)
      target_include_directories(shards-module-http PUBLIC $<TARGET_PROPERTY:Boost::beast,INTERFACE_INCLUDE_DIRECTORIES>)
    endif()
  endif()

  if(WIN32)
    # Indirect rust windows dependency
    target_link_libraries(shards-http-rust INTERFACE RuntimeObject)
  endif()

  if(EMSCRIPTEN)
    em_link_js_library(shards-module-http http.js)
    target_link_options(shards-module-http PUBLIC
      "SHELL:-s FETCH=1")
  endif()
endif()
