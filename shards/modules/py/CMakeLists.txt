if(ENABLE_PYTHON_SHARDS)
  set(SOURCES
    py.cpp
  )

  add_shards_module(py
    SOURCES ${SOURCES}
    REGISTER_SHARDS py)

  target_link_libraries(shards-module-py Boost::process)
endif()