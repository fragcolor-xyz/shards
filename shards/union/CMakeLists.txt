shards_generate_union(shards-union)
shards_generate_rust_union(shards-rust-union)

# Insert dependency that fixes some C++ that call exposed rust functions
set_property(TARGET shards-union APPEND PROPERTY INTERFACE_LINK_LIBRARIES shards-rust-union)
set_property(TARGET shards-rust-union-rust APPEND PROPERTY INTERFACE_LINK_LIBRARIES OpenSSL)
