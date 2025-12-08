if(ENABLE_PYTHON_SHARDS AND NOT IOS AND NOT VISIONOS AND NOT WATCHOS)
  set(SOURCES
    py.cpp
  )

  add_shards_module(py
    SOURCES ${SOURCES}
    REGISTER_SHARDS py)

  target_link_libraries(shards-module-py Boost::process)
endif()