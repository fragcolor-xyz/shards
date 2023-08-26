shards_generate_union(shards-union)
shards_generate_rust_union(shards-rust-union
  RUST_TARGETS shards-rust shards-macro-rust
)

# Manually add shards source files as dependencies
set(RUST_PATH ../rust)
set(RUST_MACRO_PATH ../rust)
file(GLOB_RECURSE EGUI_DEPS ${EGUI_PATH}/*.rs ${EGUI_PATH}/Cargo.toml)
file(GLOB_RECURSE EGUI_DEPS_REMOVE ${EGUI_PATH}/target/*.rs)
message(DEBUG ${EGUI_DEPS_REMOVE})
list(REMOVE_ITEM EGUI_DEPS ${EGUI_DEPS_REMOVE})

# Insert dependency that fixes some C++ that call exposed rust functions
set_property(TARGET shards-union APPEND PROPERTY INTERFACE_LINK_LIBRARIES shards-rust-union)

rust_copy_cargo_lock(shards-rust-union Cargo.lock)
