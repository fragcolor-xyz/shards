set(SOURCES
  inputs.cpp
  detached.cpp
)

is_module_enabled(HAS_EGUI egui)
if(HAS_EGUI)
  add_rust_library(NAME shards-inputs-debug-ui
    PROJECT_PATH ${CMAKE_CURRENT_LIST_DIR}/debug_ui
    DEPENDS debug_ui.hpp)
  set(INPUT_RUST_TARGETS shards-inputs-debug-ui-rust)
  set(INPUT_RUST_REGISTER debug)
  list(APPEND SOURCES debug_ui.cpp)
endif()

add_shards_module(inputs SOURCES ${SOURCES}
  REGISTER_SHARDS inputs inputs_detached ${INPUT_RUST_REGISTER}
  RUST_TARGETS ${INPUT_RUST_TARGETS})
target_link_libraries(shards-module-inputs gfx-core input)

