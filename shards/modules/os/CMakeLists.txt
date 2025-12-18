if(NOT EMSCRIPTEN AND NOT IOS AND NOT VISIONOS AND NOT WATCHOS)
  set(SOURCES
    process.cpp
  )

  add_shards_module(os SOURCES ${SOURCES}
    REGISTER_SHARDS process
    EXPERIMENTAL)

  target_link_libraries(shards-module-os Boost::process)
endif()
