set(SOURCES
  tui.cpp
)

add_shards_module(tui SOURCES ${SOURCES}
REGISTER_SHARDS tui)

target_include_directories(shards-module-tui PRIVATE ${SHARDS_DIR}/deps/FTXUI/include)
target_link_libraries(shards-module-tui ftxui::screen ftxui::dom ftxui::component)
