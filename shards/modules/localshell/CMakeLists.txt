# LocalShell module requires PTY support, not available on Emscripten
if(NOT EMSCRIPTEN AND NOT IOS AND NOT VISIONOS AND NOT WATCHOS)
  add_rust_library(NAME shards-localshell
    PROJECT_PATH ${CMAKE_CURRENT_LIST_DIR})

  add_shards_module(localshell
    RUST_TARGETS shards-localshell-rust
    REGISTER_SHARDS rust)
endif()
