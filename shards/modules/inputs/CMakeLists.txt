set(SOURCES
  inputs.cpp
  detached.cpp
)

add_rust_library(NAME shards-inputs-debug-ui 
  PROJECT_PATH ${CMAKE_CURRENT_LIST_DIR}/debug_ui
  DEPENDS debug_ui.hpp)

add_shards_module(inputs SOURCES ${SOURCES}
  REGISTER_SHARDS inputs inputs1
  RUST_TARGETS shards-inputs-debug-ui-rust)
target_link_libraries(shards-module-inputs gfx input)
