shards_generate_union(shards-union)
shards_generate_rust_union(shards-rust-union)

# Insert dependency that fixes some C++ that call exposed rust functions
target_link_libraries(shards-union shards-rust-union)
