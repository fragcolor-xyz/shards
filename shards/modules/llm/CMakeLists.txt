if(NOT EMSCRIPTEN)
  set(SOURCES
    embedd.cpp
    chat.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../deps/llama.cpp/common/common.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../deps/llama.cpp/common/sampling.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../deps/llama.cpp/common/log.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../deps/llama.cpp/examples/llava/clip.cpp
  )

  add_shards_module(llm SOURCES ${SOURCES}
    REGISTER_SHARDS llm llm_chat)

  target_link_libraries(shards-module-llm llama)
  
  # Make sure clip.h is in the include path
  target_include_directories(shards-module-llm PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../../deps/llama.cpp/examples/llava ${CMAKE_CURRENT_SOURCE_DIR}/../../../deps/llama.cpp/common)
  target_compile_definitions(shards-module-llm PRIVATE SKIP_STB_IMAGE_IMPLEMENTATION)
endif()
