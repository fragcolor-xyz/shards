if(NOT EMSCRIPTEN)
  add_rust_library(NAME shards-codex
    PROJECT_PATH ${CMAKE_CURRENT_LIST_DIR})

  add_shards_module(codex
    RUST_TARGETS shards-codex-rust
    REGISTER_SHARDS rust
    EXPERIMENTAL)
endif()
