shards_generate_union(shards-cpp-union)
shards_generate_rust_union(shards-rust-union
  RUST_TARGETS shards-rust
)

add_library(shards-union INTERFACE)

# Insert dependency that fixes some C++ that call exposed rust functions
target_link_libraries(shards-union INTERFACE shards-cpp-union shards-rust-union)

if(NOT SHARDS_NO_RUST_UNION)
  rust_copy_cargo_lock(shards-rust-union Cargo.lock)
endif()


if(EMSCRIPTEN)
  set(UNWIND_FUNCTIONS 
    # shards::channels::Broadcast::activate
    # shards::channels::Consume::activate
    # shards::channels::Listen::activate
    # shards::CaptureLog::activateWithSuspend
    # shards::Time::Pop::activate
    # shards::Wait::activate
    # shards::RunWire::activate
    shards::suspend
  )
  foreach(UF ${UNWIND_FUNCTIONS})
    list(APPEND TMP_QUOTED "'${UF}'")
  endforeach()
  
  string(JOIN "," TMP_ASYNCIFY_ADD ${TMP_QUOTED})
  target_compile_options(shards-union INTERFACE
    "SHELL:-s ASYNCIFY_ADVISE=1"
    "SHELL:-s ASYNCIFY_IGNORE_INDIRECT=1"
    "SHELL:-s \"ASYNCIFY_ADD=[${TMP_ASYNCIFY_ADD}]\""
  )
  target_link_options(shards-union INTERFACE
    "SHELL:-s ASYNCIFY_ADVISE=1"
    "SHELL:-s ASYNCIFY_IGNORE_INDIRECT=1"
    "SHELL:-s \"ASYNCIFY_ADD=[${TMP_ASYNCIFY_ADD}]\""
  )
endif()
