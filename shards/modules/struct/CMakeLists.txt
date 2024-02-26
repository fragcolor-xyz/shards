set(SOURCES
  struct.cpp
)

add_shards_module(struct SOURCES ${SOURCES}
  REGISTER_SHARDS struct)


if(ENABLE_LIBFFI)
  # Add ENABLE_LIBFFI as a preprocessor definition
  add_compile_definitions(ENABLE_LIBFFI=1)

  # Define the libffi external project
  ExternalProject_Add(libffi
    URL https://github.com/libffi/libffi/releases/download/v3.4.6/libffi-3.4.6.tar.gz
    PREFIX ${CMAKE_BINARY_DIR}/external/libffi # Where to download and build libffi
    CONFIGURE_COMMAND <SOURCE_DIR>/configure --prefix=<INSTALL_DIR> --disable-shared --enable-static --with-pic
  )

  # Make sure your project depends on libffi being built
  add_dependencies(shards-module-struct libffi)

  # Include libffi headers and link against it
  include_directories(${CMAKE_BINARY_DIR}/external/libffi/include)
  link_directories(${CMAKE_BINARY_DIR}/external/libffi/lib)

  # Assuming you have a target for your project
  target_link_libraries(shards-module-struct ffi)
endif()