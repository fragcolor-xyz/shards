# LLM module requires llama.cpp and whisper.cpp, only available on desktop platforms
is_module_enabled(LLM_ENABLED llm)
if(LLM_ENABLED AND NOT EMSCRIPTEN AND NOT WATCHOS)
  set(SOURCES
    embedd.cpp
    chat.cpp
    sh_whisper.cpp
    ${llama.cpp_SOURCE_DIR}/common/common.cpp
    ${llama.cpp_SOURCE_DIR}/common/sampling.cpp
    ${llama.cpp_SOURCE_DIR}/common/log.cpp
    ${llama.cpp_SOURCE_DIR}/tools/mtmd/clip.cpp
    ${llama.cpp_SOURCE_DIR}/tools/mtmd/mtmd.cpp
    ${llama.cpp_SOURCE_DIR}/tools/mtmd/mtmd-audio.cpp
    ${whisper.cpp_SOURCE_DIR}/src/whisper.cpp
  )

  add_shards_module(llm SOURCES ${SOURCES}
    REGISTER_SHARDS llm llm_chat whisper)

  target_include_directories(shards-module-llm PRIVATE ${whisper.cpp_SOURCE_DIR}/include)
  target_link_libraries(shards-module-llm llama)
  target_compile_definitions(shards-module-llm PRIVATE WHISPER_VERSION="shards-whisper-0.1")

  # Make sure clip.h and llama headers are in the include path
  target_include_directories(shards-module-llm PRIVATE
    ${llama.cpp_SOURCE_DIR}
    ${llama.cpp_SOURCE_DIR}/examples/llava
    ${llama.cpp_SOURCE_DIR}/common)
  target_compile_definitions(shards-module-llm PRIVATE SKIP_STB_IMAGE_IMPLEMENTATION LLAVA_LOG_OFF)
endif()
