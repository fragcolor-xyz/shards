if(NOT EMSCRIPTEN)
  set(SOURCES
    embedd.cpp
    chat.cpp
    ${CMAKE_SOURCE_DIR}/deps/llama.cpp/common/common.cpp
    ${CMAKE_SOURCE_DIR}/deps/llama.cpp/common/sampling.cpp
    ${CMAKE_SOURCE_DIR}/deps/llama.cpp/examples/llava/clip.cpp
  )

  add_shards_module(llm SOURCES ${SOURCES}
    REGISTER_SHARDS llm llm_chat)

  target_link_libraries(shards-module-llm llama)
  
  # Make sure clip.h is in the include path
  target_include_directories(shards-module-llm PRIVATE ${CMAKE_SOURCE_DIR}/deps/llama.cpp/examples/llava ${CMAKE_SOURCE_DIR}/deps/llama.cpp/common)
endif()
