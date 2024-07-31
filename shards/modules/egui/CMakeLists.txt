set(SOURCES
  egui_types.cpp
  renderer.cpp
  input.cpp
  rust_interop.cpp
  pass.cpp)

add_library(egui-gfx-integration ${SOURCES})
target_link_libraries(egui-gfx-integration gfx-core shards-core)
target_include_directories(egui-gfx-integration PUBLIC ../../egui)
target_compile_definitions(egui-gfx-integration PRIVATE SHARDS_THIS_MODULE_ID=egui) # Fake module id

# Manually add egui source files as dependencies
set(EGUI_PATH ../../egui)
file(GLOB_RECURSE EGUI_DEPS ${EGUI_PATH}/*.rs ${EGUI_PATH}/Cargo.toml)
file(GLOB_RECURSE EGUI_DEPS_REMOVE ${EGUI_PATH}/target/*.rs)
message(DEBUG ${EGUI_DEPS_REMOVE})
list(REMOVE_ITEM EGUI_DEPS ${EGUI_DEPS_REMOVE})

add_rust_library(NAME shards-egui-register
  PROJECT_PATH ${CMAKE_CURRENT_LIST_DIR}
  DEPENDS ${EGUI_DEPS})
target_link_libraries(shards-egui-register-rust INTERFACE egui-gfx-integration)

add_shards_module(egui
  RUST_TARGETS shards-egui-register-rust
  REGISTER_SHARDS egui pass)
  
# target_link_libraries(shards-module-egui egui-gfx-integration shards-egui-register-rust)
