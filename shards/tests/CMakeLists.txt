if(SHARDS_BUILD_TESTS)
  set(TESTS_SRC ../tests)
  add_executable(test-runtime
    ${TESTS_SRC}/test_runtime.cpp)

  target_link_libraries(test-runtime
    shards-union shards-lang-ffi-rust Catch2 Catch2Main
  )

  if(EMSCRIPTEN)
    target_link_options(test-runtime PUBLIC
      "SHELL:-s MODULARIZE=1"
      "SHELL:-s EXPORT_NAME=test"
      "SHELL:-s NO_EXIT_RUNTIME=1"
    )
  endif()
endif()
