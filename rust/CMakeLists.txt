set(FEATURES shards)

if(RUST_SHLISP)
  list(APPEND FEATURES scripting)
endif()

# Bindgen dependency
list(APPEND EXTRA_SOURCES ${SHARDS_DIR}/src/extra/rust_interop.hpp ${SHARDS_DIR}/include/shards.h)

# egui integration sources
file(GLOB_RECURSE EXTRA_SOURCES "${SHARDS_DIR}/src/gfx/egui/src/*.rs")

if(ANDROID OR LINUX)
  set(NEED_OPENSSL_SYS ON)
endif()

if(NEED_OPENSSL_SYS)
  get_property(SSL_DIR GLOBAL PROPERTY OPENSSL_SOURCE_DIR)
  set(SSL_ENV OPENSSL_DIR=${SSL_DIR} OPENSSL_INCLUDE_DIR=${SSL_DIR}/include OPENSSL_LIBS=$<TARGET_FILE_BASE_NAME:ssl> OPENSSL_LIB_DIR=$<TARGET_FILE_DIR:ssl> OPENSSL_STATIC=1)
endif()

add_rust_library(
  NAME shards
  PROJECT_PATH ${CMAKE_CURRENT_LIST_DIR}
  TARGET_PATH ${CMAKE_CURRENT_LIST_DIR}/../target
  FEATURES ${FEATURES}
  ENVIRONMENT ${SSL_ENV}
  DEPENDS ${EXTRA_SOURCES}
)

if(WIN32)
  target_link_libraries(shards-rust INTERFACE Userenv)
  target_link_libraries(shards-rust INTERFACE ws2_32 Bcrypt Crypt32 Secur32 NtDll Ncrypt)
endif()

if(NEED_OPENSSL_SYS)
  add_dependencies(cargo-shards-rust OpenSSL)
endif()

if(NOT (APPLE OR EMSCRIPTEN OR WIN32))
  # Required by openssl-sys
  target_link_libraries(shards-rust INTERFACE OpenSSL)
endif()
