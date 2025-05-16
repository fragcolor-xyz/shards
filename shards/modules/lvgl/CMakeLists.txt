set(SOURCES
  lvgl.cpp
)

add_shards_module(lvgl SOURCES ${SOURCES}
  REGISTER_SHARDS lvgl
)

target_link_libraries(shards-module-lvgl gfx-core input lvgl::lvgl)
