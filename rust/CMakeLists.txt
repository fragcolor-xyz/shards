unset(Rust_FEATURES)

ADD_RUST_FEATURE(Rust_FEATURES blocks)

if(NOT SKIP_RUST_BINDGEN)
  # So we got issues when building win32 and libclang...
  # let's bindgen on win64 only and the other OSs
  ADD_RUST_FEATURE(Rust_FEATURES run_bindgen)
endif()

if(RUST_CBLISP)
  ADD_RUST_FEATURE(Rust_FEATURES cblisp)
endif()

if(Rust_CARGO_TARGET)
  set(TARGET_SWITCH --target ${Rust_CARGO_TARGET})
endif()

set(GENERATED_LIB_PATH ${CHAINBLOCKS_DIR}/target/${Rust_BUILD_SUBDIR}/${Rust_LIB_PREFIX}chainblocks${Rust_LIB_SUFFIX})

file(GLOB_RECURSE SOURCES src/*.rs *.toml)

# Bindgen dependency
if(NOT SKIP_RUST_BINDGEN)
  list(APPEND SOURCES ${CHAINBLOCKS_DIR}/include/chainblocks.h)
endif()

if(Rust_FEATURES)
  set(FEATURES_ARG --features ${Rust_FEATURES})
endif()

if(IOS)
  set(BUILD_SCRIPT "${CMAKE_SOURCE_DIR}/cmake/osx_rust_build.sh")
endif()

if(ANDROID OR LINUX)
  list(APPEND Rust_CARGO_UNSTABLE_FLAGS --crate-type staticlib)
  set(NEED_OPENSSL_SYS ON)
endif()

if(NEED_OPENSSL_SYS)
  set(SSL_DIR ${CHAINBLOCKS_DIR}/deps/libressl)
  set(SSL_ENV OPENSSL_DIR=${SSL_DIR} OPENSSL_INCLUDE_DIR=${SSL_DIR}/include OPENSSL_LIBS=$<TARGET_FILE_BASE_NAME:ssl> OPENSSL_LIB_DIR=$<TARGET_FILE_DIR:ssl> OPENSSL_STATIC=1)
endif()

add_custom_command(
  OUTPUT ${GENERATED_LIB_PATH}
  COMMAND ${CMAKE_COMMAND} -E env RUSTFLAGS="${Rust_FLAGS}" ${SSL_ENV} ${BUILD_SCRIPT} cargo ${Rust_CARGO_TOOLCHAIN} rustc ${Rust_CARGO_UNSTABLE_FLAGS} ${FEATURES_ARG} ${TARGET_SWITCH} ${Rust_CARGO_FLAGS}
  WORKING_DIRECTORY ${CHAINBLOCKS_DIR}/rust
  DEPENDS ${SOURCES}
  USES_TERMINAL
)
add_custom_target(
  cargo-chainblocks-rust
  DEPENDS ${GENERATED_LIB_PATH}
)

if(NEED_OPENSSL_SYS)
  add_dependencies(cargo-chainblocks-rust ssl)
endif()

add_library(chainblocks-rust STATIC IMPORTED GLOBAL)
add_dependencies(chainblocks-rust cargo-chainblocks-rust)
set_target_properties(chainblocks-rust PROPERTIES
  IMPORTED_LOCATION ${GENERATED_LIB_PATH}
)

if(WIN32)
  target_link_libraries(chainblocks-rust INTERFACE Userenv)
  target_link_libraries(chainblocks-rust INTERFACE ws2_32 Bcrypt Crypt32 Secur32 NtDll Ncrypt)
endif()