if(SHARDS_DEBUGGER)
  add_rust_library(NAME shards-debugger
    PROJECT_PATH ${CMAKE_CURRENT_LIST_DIR})
  add_shards_module(debugger
    SOURCES interface.cpp debugger.cpp
    RUST_TARGETS shards-debugger-rust)

  target_link_libraries(shards-module-debugger
    Boost::filesystem Boost::container Boost::beast shards-logging
    magic_enum nameof nlohmann_json entt
  )
  target_include_directories(shards-module-debugger PRIVATE ${SHARDS_ROOT}/include)
endif()