set(FEATURES shards)

if(RUST_SHLISP)
  list(APPEND FEATURES scripting)
endif()

# Bindgen dependency
list(APPEND EXTRA_SOURCES ${SHARDS_DIR}/shards/gfx/rust_interop.hpp ${SHARDS_DIR}/include/shards/shards.h)

# egui integration sources
file(GLOB_RECURSE EXTRA_SOURCES "${SHARDS_DIR}/shards/modules/egui/src/*.rs")

if(ANDROID OR DESKTOP_LINUX)
  set(NEED_OPENSSL_SYS ON)
endif()

if(NEED_OPENSSL_SYS)
  # If we build libressl, explicitly specify the library & include paths
  get_property(libressl_SOURCE_DIR GLOBAL PROPERTY libressl_SOURCE_DIR)
  if(libressl_SOURCE_DIR)
    set(SSL_ENV OPENSSL_DIR=${libressl_SOURCE_DIR} OPENSSL_INCLUDE_DIR=${libressl_SOURCE_DIR}/include OPENSSL_LIBS=$<TARGET_FILE_BASE_NAME:ssl> OPENSSL_LIB_DIR=$<TARGET_FILE_DIR:ssl> OPENSSL_STATIC=1)
  endif()
endif()

add_rust_library(
  NAME shards
  PROJECT_PATH ${CMAKE_CURRENT_LIST_DIR}
  FEATURES ${FEATURES}
  ENVIRONMENT ${SSL_ENV}
  DEPENDS ${EXTRA_SOURCES}
)

if(WIN32)
  target_link_libraries(shards-rust INTERFACE Userenv)
  target_link_libraries(shards-rust INTERFACE ws2_32 Bcrypt Crypt32 Secur32 NtDll Ncrypt)
endif()

if(NEED_OPENSSL_SYS)
  target_link_libraries(shards-rust INTERFACE OpenSSL)
endif()
