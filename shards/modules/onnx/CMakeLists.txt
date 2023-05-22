add_shards_module(onnx 
  REGISTER_SHARDS rust
  EXPERIMENTAL)

add_rust_library(NAME shards-onnx 
  PROJECT_PATH ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(shards-module-onnx shards-onnx-rust)
