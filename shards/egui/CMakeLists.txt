add_rust_library(NAME shards-egui
  PROJECT_PATH ${CMAKE_CURRENT_LIST_DIR}
  DEPENDS 
    rust_interop.hpp
    context.hpp
    egui_types.hpp
    input.hpp
    renderer.hpp)
