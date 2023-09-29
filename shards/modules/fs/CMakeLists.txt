set(SOURCES
  fs.cpp
)

if(DESKTOP AND (APPLE OR WIN32))
  list(APPEND FS_RUST_FEATURES "rfd-enabled")
  list(APPEND FS_RUST_FEATURES "rfd-xdg")
elseif(RFD_GTK3)
  list(APPEND FS_RUST_FEATURES "rfd-enabled")
  list(APPEND FS_RUST_FEATURES "rfd-gtk3")
endif()

add_rust_library(NAME shards-fs
  PROJECT_PATH ${CMAKE_CURRENT_LIST_DIR}
  FEATURES ${FS_RUST_FEATURES})

add_shards_module(fs SOURCES ${SOURCES}
  REGISTER_SHARDS fs rust
  RUST_TARGETS shards-fs-rust)

target_link_libraries(shards-module-fs Boost::filesystem)

if(RFD_GTK3)
  # add gtk3 libraries
  target_link_libraries(shards-module-fs gtk-3 gobject-2.0)
endif()
