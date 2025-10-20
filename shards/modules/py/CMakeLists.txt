if(ENABLE_PYTHON_SHARDS)
  # Option to use embedded RustPython instead of dynamic Python loading
  option(ENABLE_RUSTPYTHON_EMBEDDED "Use embedded RustPython for Py.Eval shard" OFF)

  # Conditionally build RustPython implementation
  if(ENABLE_RUSTPYTHON_EMBEDDED)
    add_rust_library(NAME shards-py
      PROJECT_PATH ${CMAKE_CURRENT_LIST_DIR})
    add_shards_module(py
      RUST_TARGETS shards-py-rust
      REGISTER_SHARDS py)
  else()
    set(SOURCES
      py.cpp
    )

    add_shards_module(py
      SOURCES ${SOURCES}
      REGISTER_SHARDS py)

    target_link_libraries(shards-module-py Boost::process)
  endif()
endif()