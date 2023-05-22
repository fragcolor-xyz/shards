set(SOURCES
  egui_types.cpp
  renderer.cpp
  input.cpp
  rust_interop.cpp
  pass.cpp)

add_shards_module(egui 
  SOURCES ${SOURCES}
  REGISTER_SHARDS egui pass)

add_rust_library(NAME shards-egui 
  PROJECT_PATH ${CMAKE_CURRENT_LIST_DIR}
  DEPENDS rust_interop.hpp)
target_link_libraries(shards-module-egui gfx shards-egui-rust)
target_link_libraries(shards-egui-rust INTERFACE shards-module-egui)
