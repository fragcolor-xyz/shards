if(ANDROID OR DESKTOP_LINUX OR APPLE)
  set(NEED_OPENSSL_SYS ON)
endif()

if(NEED_OPENSSL_SYS)
  # If we build libressl, explicitly specify the library & include paths
  get_property(libressl_SOURCE_DIR GLOBAL PROPERTY libressl_SOURCE_DIR)

  if(libressl_SOURCE_DIR)
    set(SSL_ENV OPENSSL_DIR=${libressl_SOURCE_DIR} OPENSSL_INCLUDE_DIR=${libressl_SOURCE_DIR}/include OPENSSL_LIBS=$<TARGET_FILE_BASE_NAME:ssl> OPENSSL_LIB_DIR=$<TARGET_FILE_DIR:ssl> OPENSSL_STATIC=1)
  endif()
endif()

add_rust_library(NAME shards-ssh
  PROJECT_PATH ${CMAKE_CURRENT_LIST_DIR}
  ENVIRONMENT ${SSL_ENV})

add_shards_module(ssh
  RUST_TARGETS shards-ssh-rust
  REGISTER_SHARDS rust)

# Link system zlib for libssh2 compression support
if(APPLE OR LINUX)
  target_link_libraries(shards-ssh-rust INTERFACE "-lz")
endif()
