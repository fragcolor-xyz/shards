# LLM module requires llama.cpp and whisper.cpp, only available on desktop platforms
is_module_enabled(LLM_ENABLED llm)
if(LLM_ENABLED AND NOT EMSCRIPTEN AND NOT WATCHOS)
  # Collect all mtmd model sources via glob
  file(GLOB MTMD_MODEL_SOURCES ${llama.cpp_SOURCE_DIR}/tools/mtmd/models/*.cpp)

  set(SOURCES
    embedd.cpp
    chat.cpp
    sh_whisper.cpp
    ${llama.cpp_SOURCE_DIR}/common/common.cpp
    ${llama.cpp_SOURCE_DIR}/common/sampling.cpp
    ${llama.cpp_SOURCE_DIR}/common/log.cpp
    ${llama.cpp_SOURCE_DIR}/common/unicode.cpp
    ${llama.cpp_SOURCE_DIR}/common/reasoning-budget.cpp
    ${llama.cpp_SOURCE_DIR}/tools/mtmd/clip.cpp
    ${llama.cpp_SOURCE_DIR}/tools/mtmd/mtmd.cpp
    ${llama.cpp_SOURCE_DIR}/tools/mtmd/mtmd-audio.cpp
    ${llama.cpp_SOURCE_DIR}/tools/mtmd/mtmd-image.cpp
    ${llama.cpp_SOURCE_DIR}/tools/mtmd/mtmd-helper.cpp
    ${MTMD_MODEL_SOURCES}
    ${whisper.cpp_SOURCE_DIR}/src/whisper.cpp
  )

  add_shards_module(llm SOURCES ${SOURCES}
    REGISTER_SHARDS llm llm_chat whisper)

  target_include_directories(shards-module-llm PRIVATE ${whisper.cpp_SOURCE_DIR}/include)
  target_link_libraries(shards-module-llm llama)
  target_compile_definitions(shards-module-llm PRIVATE WHISPER_VERSION="shards-whisper-0.1")

  # Make sure clip.h, llama, mtmd, and vendor headers are in the include path
  target_include_directories(shards-module-llm PRIVATE
    ${llama.cpp_SOURCE_DIR}
    ${llama.cpp_SOURCE_DIR}/common
    ${llama.cpp_SOURCE_DIR}/tools/mtmd
    ${llama.cpp_SOURCE_DIR}/vendor)
  target_compile_definitions(shards-module-llm PRIVATE SKIP_STB_IMAGE_IMPLEMENTATION LLAVA_LOG_OFF)
endif()
