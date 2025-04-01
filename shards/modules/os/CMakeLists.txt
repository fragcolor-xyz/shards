if(NOT EMSCRIPTEN AND NOT IOS AND NOT VISIONOS)
  set(SOURCES
    os.cpp
    process.cpp
  )

  add_shards_module(os SOURCES ${SOURCES}
    REGISTER_SHARDS os process
    EXPERIMENTAL)

  target_link_libraries(shards-module-os Boost::process)
endif()
