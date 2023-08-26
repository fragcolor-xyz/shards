shards_generate_union(shards-union)
shards_generate_rust_union(shards-rust-union
  RUST_TARGETS shards-rust shards-macro-rust
)

# Insert dependency that fixes some C++ that call exposed rust functions
set_property(TARGET shards-union APPEND PROPERTY INTERFACE_LINK_LIBRARIES shards-rust-union)

rust_copy_cargo_lock(shards-rust-union Cargo.lock)
