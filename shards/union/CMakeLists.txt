shards_generate_union(shards-cpp-union)
shards_generate_rust_union(shards-rust-union
  RUST_TARGETS shards-rust
)

# Setup the circular dependency (only when CMake builds Rust)
if(NOT SHARDS_NO_RUST_UNION)
  target_link_libraries(shards-cpp-union shards-rust-union-rust)
  target_link_libraries(shards-rust-union-rust INTERFACE shards-cpp-union)
  rust_copy_cargo_lock(shards-rust-union Cargo.lock)
endif()
