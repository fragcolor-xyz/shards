shards_generate_union(shards-cpp-union)
shards_generate_rust_union(shards-rust-union
  RUST_TARGETS shards-rust shards-macro-rust
)

add_library(shards-union INTERFACE)

# Insert dependency that fixes some C++ that call exposed rust functions
target_link_libraries(shards-union INTERFACE shards-cpp-union shards-rust-union)

rust_copy_cargo_lock(shards-rust-union Cargo.lock)
