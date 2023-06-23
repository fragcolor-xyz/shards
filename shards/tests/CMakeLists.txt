if(SHARDS_BUILD_TESTS)
  add_executable(test-runtime ../tests/test_runtime.cpp)

  target_link_libraries(test-runtime
    shards-union shards-lang-rust Catch2 Catch2Main
  )

  if(EMSCRIPTEN)
    target_link_options(test-runtime PUBLIC
      "SHELL:-s MODULARIZE=1"
      "SHELL:-s EXPORT_NAME=test"
      "SHELL:-s NO_EXIT_RUNTIME=1"
    )
  endif()
endif()
