if(NOT EMSCRIPTEN)
  set(SOURCES
    embedd.cpp
    chat.cpp
  )

  add_shards_module(llm SOURCES ${SOURCES}
    REGISTER_SHARDS llm)

  target_link_libraries(shards-module-llm llama)
  
  # Make sure clip.h is in the include path
  target_include_directories(shards-module-llm PRIVATE ${CMAKE_SOURCE_DIR}/deps/llama.cpp/examples/llava ${CMAKE_SOURCE_DIR}/deps/llama.cpp/common)
endif()
