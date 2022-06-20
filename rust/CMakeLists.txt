unset(Rust_FEATURES)

ADD_RUST_FEATURE(Rust_FEATURES shards)

if(NOT SKIP_RUST_BINDGEN)
  # So we got issues when building win32 and libclang...
  # let's bindgen on win64 only and the other OSs
  ADD_RUST_FEATURE(Rust_FEATURES run_bindgen)
endif()

if(RUST_SHLISP)
  ADD_RUST_FEATURE(Rust_FEATURES scripting)
endif()

if(Rust_CARGO_TARGET)
  set(TARGET_SWITCH --target ${Rust_CARGO_TARGET})
endif()

set(GENERATED_LIB_PATH ${SHARDS_DIR}/target/${Rust_BUILD_SUBDIR}/${Rust_LIB_PREFIX}shards${Rust_LIB_SUFFIX})

file(GLOB_RECURSE SOURCES src/*.rs *.toml)

# Bindgen dependency
if(NOT SKIP_RUST_BINDGEN)
  list(APPEND SOURCES ${SHARDS_DIR}/include/shards.h)
endif()

if(Rust_FEATURES)
  set(FEATURES_ARG --features ${Rust_FEATURES})
endif()

if(IOS)
  set(BUILD_SCRIPT "${CMAKE_SOURCE_DIR}/cmake/osx_rust_build.sh")
endif()

if(ANDROID OR LINUX)
  set(NEED_OPENSSL_SYS ON)
endif()

if(NEED_OPENSSL_SYS)
  get_property(SSL_DIR GLOBAL PROPERTY OPENSSL_SOURCE_DIR)
  set(SSL_ENV OPENSSL_DIR=${SSL_DIR} OPENSSL_INCLUDE_DIR=${SSL_DIR}/include OPENSSL_LIBS=$<TARGET_FILE_BASE_NAME:ssl> OPENSSL_LIB_DIR=$<TARGET_FILE_DIR:ssl> OPENSSL_STATIC=1)
endif()

if(IOS)
  set(BUILD_SCRIPT "${SHARDS_DIR}/cmake/osx_rust_build.sh")
endif()

add_custom_command(
  OUTPUT ${GENERATED_LIB_PATH}
  COMMAND ${CMAKE_COMMAND} -E env RUSTFLAGS="${Rust_FLAGS}" ${SSL_ENV} ${BUILD_SCRIPT} ${CARGO_EXE} ${Rust_CARGO_TOOLCHAIN} rustc ${Rust_CARGO_UNSTABLE_FLAGS} --crate-type staticlib ${FEATURES_ARG} ${TARGET_SWITCH} ${Rust_CARGO_FLAGS}
  WORKING_DIRECTORY ${SHARDS_DIR}/rust
  DEPENDS ${SOURCES}
  USES_TERMINAL
)
add_custom_target(
  cargo-shards-rust
  DEPENDS ${GENERATED_LIB_PATH}
)

if(NEED_OPENSSL_SYS)
  add_dependencies(cargo-shards-rust ssl)
endif()

add_library(shards-rust STATIC IMPORTED GLOBAL)
add_dependencies(shards-rust cargo-shards-rust)
set_target_properties(shards-rust PROPERTIES
  IMPORTED_LOCATION ${GENERATED_LIB_PATH}
)

if(WIN32)
  target_link_libraries(shards-rust INTERFACE Userenv)
  target_link_libraries(shards-rust INTERFACE ws2_32 Bcrypt Crypt32 Secur32 NtDll Ncrypt)
endif()

if(NOT (APPLE OR EMSCRIPTEN OR WIN32))
  # Required by openssl-sys
  target_link_libraries(shards-rust INTERFACE OpenSSL)
endif()
