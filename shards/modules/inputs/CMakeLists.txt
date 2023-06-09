set(SOURCES
  inputs.cpp
  inputs1.cpp
)

add_shards_module(inputs SOURCES ${SOURCES}
  REGISTER_SHARDS inputs inputs1)

target_link_libraries(shards-module-inputs gfx input)

add_rust_library(NAME shards-inputs-debug-ui 
  PROJECT_PATH ${CMAKE_CURRENT_LIST_DIR}/debug_ui
  DEPENDS debug_ui.hpp)
target_link_libraries(shards-module-inputs shards-inputs-debug-ui-rust)
