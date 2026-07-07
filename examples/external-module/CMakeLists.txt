# An example external Shards module, statically linked into the runtime.
#
# A single module can carry both C++ sources and a Rust crate; each language
# contributes its own registration function (REGISTER_SHARDS entries expand to
# shardsRegister_<module>_<id>, see cmake/Modules.cmake).
#
# Enable by configuring shards with:
#   -DSHARDS_EXTERNAL_MODULE_DIRS=<path-to-this-directory>

add_rust_library(NAME shards-example
  PROJECT_PATH ${CMAKE_CURRENT_LIST_DIR}/rust)

add_shards_module(example
  SOURCES cpp/example.cpp
  RUST_TARGETS shards-example-rust
  REGISTER_SHARDS cpp rust)
