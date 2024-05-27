if(RUST_IS_NIGHTLY)
  add_rust_library(NAME shards-onnx
    PROJECT_PATH ${CMAKE_CURRENT_LIST_DIR})

  add_shards_module(onnx
    REGISTER_SHARDS rust
    RUST_TARGETS shards-onnx-rust
    EXPERIMENTAL)
else()
  message(WARNING "ONNX module can only be built with nightly rust")
endif()