set(SOURCES
  egui_types.cpp
  renderer.cpp
  input.cpp
  rust_interop.cpp
  pass.cpp
  debug_ui.cpp
)

# Manually add egui source files as dependencies
set(EGUI_PATH ../../egui)
file(GLOB_RECURSE EGUI_DEPS ${EGUI_PATH}/*.rs ${EGUI_PATH}/Cargo.toml)
file(GLOB_RECURSE EGUI_DEPS_REMOVE ${EGUI_PATH}/target/*.rs)
message(DEBUG ${EGUI_DEPS_REMOVE})
list(REMOVE_ITEM EGUI_DEPS ${EGUI_DEPS_REMOVE})

add_rust_library(NAME shards-inputs-debug-ui
  PROJECT_PATH ${CMAKE_CURRENT_LIST_DIR}/debug_ui
  DEPENDS debug_ui.hpp)
list(APPEND SOURCES debug_ui.cpp)

add_rust_library(NAME shards-egui-register
  PROJECT_PATH ${CMAKE_CURRENT_LIST_DIR}
  DEPENDS ${EGUI_DEPS})

add_shards_module(egui
  SOURCES ${SOURCES}
  RUST_TARGETS shards-egui-register-rust shards-inputs-debug-ui-rust
  REGISTER_SHARDS egui pass debug)
  
target_link_libraries(shards-module-egui gfx-core shards-core)
target_include_directories(shards-module-egui PUBLIC ../../egui)
