# Platform-specific features
if(APPLE)
  list(APPEND ML_RUST_FEATURES "metal")
  list(APPEND ML_RUST_FEATURES "accelerate")
endif()

# Try to find CUDA package (optional)
find_package(CUDAToolkit QUIET)

# Check if CUDA is found
if(CUDAToolkit_FOUND)
  message(STATUS "CUDA found. Enabling CUDA support.")
  list(APPEND ML_RUST_FEATURES "cuda")
else()
  message(STATUS "CUDA not found. Skipping CUDA support.")
endif()

# Define the Rust library with the collected features
add_rust_library(NAME shards-ml
  PROJECT_PATH ${CMAKE_CURRENT_LIST_DIR}
  FEATURES ${ML_RUST_FEATURES})

# Add the Shards module
add_shards_module(ml
  REGISTER_SHARDS rust
  RUST_TARGETS shards-ml-rust
  EXPERIMENTAL)