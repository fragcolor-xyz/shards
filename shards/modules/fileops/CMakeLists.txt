if(NOT EMSCRIPTEN)
  add_rust_library(NAME shards-fileops
    PROJECT_PATH ${CMAKE_CURRENT_LIST_DIR})

  add_shards_module(fileops
    RUST_TARGETS shards-fileops-rust
    REGISTER_SHARDS rust
    EXPERIMENTAL)
endif()
