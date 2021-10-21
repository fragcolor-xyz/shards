unset(Rust_FEATURES)

ADD_RUST_FEATURE(Rust_FEATURES blocks)

if(NOT SKIP_RUST_BINDGEN)
  # So we got issues when building win32 and libclang...
  # let's bindgen on win64 only and the other OSs
  ADD_RUST_FEATURE(Rust_FEATURES run_bindgen)
endif()

if(Rust_CARGO_TARGET)
  set(TARGET_SWITCH --target ${Rust_CARGO_TARGET})
endif()

set(GENERATED_LIB_PATH ${CHAINBLOCKS_DIR}/target/${Rust_BUILD_SUBDIR}/libchainblocks.a)

file(GLOB_RECURSE SOURCES *.rs *.toml)

if(Rust_FEATURES)
  set(FEATURES_ARG --features ${Rust_FEATURES})
endif()

add_custom_command(
  OUTPUT ${GENERATED_LIB_PATH}
  COMMAND ${CMAKE_COMMAND} -E env RUSTFLAGS="${Rust_FLAGS}" cargo ${Rust_CARGO_TOOLCHAIN} build ${Rust_CARGO_UNSTABLE_FLAGS} ${FEATURES_ARG} ${TARGET_SWITCH} ${Rust_CARGO_FLAGS}
  WORKING_DIRECTORY ${CHAINBLOCKS_DIR}/rust
  DEPENDS ${SOURCES}
  USES_TERMINAL
)
add_custom_target(
  cargo-chainblocks-rust
  DEPENDS ${GENERATED_LIB_PATH}
)

add_library(chainblocks-rust STATIC IMPORTED GLOBAL)
add_dependencies(chainblocks-rust cargo-chainblocks-rust)
set_target_properties(chainblocks-rust PROPERTIES
  IMPORTED_LOCATION ${GENERATED_LIB_PATH}
)

if(WIN32)
  target_link_libraries(chainblocks-rust INTERFACE Userenv)
  target_link_libraries(chainblocks-rust INTERFACE ws2_32 Bcrypt Crypt32 Secur32 NtDll Ncrypt)
endif()