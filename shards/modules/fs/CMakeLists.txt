set(SOURCES
  fs.cpp
)

add_rust_library(NAME shards-fs
  PROJECT_PATH ${CMAKE_CURRENT_LIST_DIR})

add_shards_module(fs SOURCES ${SOURCES}
  REGISTER_SHARDS fs rust
  RUST_TARGETS shards-fs-rust)

target_link_libraries(shards-module-fs Boost::filesystem)

if(LINUX)
  # add gtk3 libraries
  target_link_libraries(shards-module-fs gtk-3 gobject-2.0)
endif()
